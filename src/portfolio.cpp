#include <cstddef>
#include <cstdio>
#include <gmp.h>
#include "pch.h"
#include <stdexcept>
#include <fmt/core.h>

#include "asset.h"
#include "exchange.h"
#include "order.h"
#include "portfolio.h"
#include "broker.h"
#include "position.h"
#include "pybind11/pytypes.h"
#include "settings.h"

#include "utils_time.h"


using portfolio_sp_t = Portfolio::portfolio_sp_t;
using exchanges_sp_t = ExchangeMap::exchanges_sp_t;
using position_sp_t = Position::position_sp_t;
using order_sp_t = Order::order_sp_t;

using namespace std;

Portfolio::Portfolio(
    int logging_, 
    double cash_, 
    string id_, 
    Portfolio* parent_portfolio_,
    brokers_sp_t brokers_,
    exchanges_sp_t exchange_map_) : positions_map()
{
    this->brokers = brokers_;
    this->parent_portfolio = parent_portfolio_;
    this->exchange_map = exchange_map_;

    this->portfolio_history = make_shared<PortfolioHistory>(this);
    this->event_tracer = nullptr;

    this->logging = logging_;
    this->cash = cash_;
    this->starting_cash = cash_;
    this->nlv = cash_;
    this->portfolio_id = std::move(id_);
}

void Portfolio::build(size_t portfolio_eval_length)
{
    this->portfolio_history->build(portfolio_eval_length);
    this->is_built = true;

    //recursively build portfolios with given size
    for(auto& portfolio_pair : this->portfolio_map){
        portfolio_pair.second->build(portfolio_eval_length);
    }
}

void Portfolio::reset(bool clear_history)
{
    // reset starting member variables
    this->cash = this->starting_cash;
    this->unrealized_pl = 0;
    this->nlv = this->starting_cash;

    // reset portfolio history object
    this->portfolio_history->reset(clear_history);

    // clear positions map
    this->positions_map.clear();

    //recursively reset all child portfolios
    for(auto& portfolio_pair : this->portfolio_map){
        //force clear of child portfolio histories
        portfolio_pair.second->reset(true);
    }
}

std::optional<position_sp_t> Portfolio::get_position(const string &asset_id)
{
    auto position = this->positions_map.find(asset_id);
    if(position == this->positions_map.end()){
        return nullopt;
    }
    else {
        return position->second;
    }
}
void Portfolio::order_target_allocations(py::dict allocations, 
                        const string &strategy_id,
                        double epsilon,
                        OrderExecutionType order_execution_type,
                        OrderTargetType order_target_type,
                        bool clear_missing)
{
    // if clear_missing, then close positions that exist which are not in the allocation map
    if(clear_missing)
    {
        for(auto& position_pair : this->positions_map)
        {
            if(!allocations.contains(position_pair.first))
            {
                std::vector<order_sp_t> orders;
                auto orders_nullopt = this->generate_order_inverse(position_pair.first, true, false);
                if(this->logging){printf("\n");}
            }
        }
    }
    // iterate over allocations and process them as needed 
    for(auto& allocation_pair : allocations)
    {
        std::string asset_id = py::str(allocation_pair.first);
        auto allocation = py::cast<double>(allocation_pair.second);

        // place the allocation using the order_target_size function (provides same functionality for indivual targets)
        this->order_target_size(
            asset_id, 
            allocation, 
            strategy_id, 
            epsilon, 
            order_target_type
        );
    }
}

void Portfolio::order_target_size(const string &asset_id_, double size,
                                const string &strategy_id,
                                double epsilon,
                                OrderTargetType order_target_type,
                                OrderExecutionType order_execution_type,
                                int trade_id)
{
    double units = size;
    auto position = this->get_position(asset_id_);
    
    double market_price = this->exchange_map->get_market_price(asset_id_);

    switch (order_target_type) {
        case UNITS:
            break;
        case DOLLARS:
            units /= market_price;
            break;
        case PCT:
            units = (size * this->get_nlv()) / market_price;
            break;
    }

    if(position.has_value())
    {
        // if position exists adjust order units
        // i.e. if currently long 10 units but target is -10, then have to subtract the 10 units.
        double existing_units = position.value()->get_units();
        units -= existing_units;

        // check to see if units needed to adjust position to correct size is greater then the epsilon passed
        double offset = abs((existing_units - units) / units);
        if(offset < epsilon)
        {
            return;
        }

        // if no trade id is passed, set it equal to the first trade id in position, prevent continuous  
        // reallignments from spam generating small trades
        trade_id = position.value()->get_trades().begin()->first;
    }

    // position is already at target size
    if(units == 0.0f)
    {
        return;
    }
    // position needs to be altered
    else
    {
        this->place_market_order(asset_id_, units, strategy_id, order_execution_type, trade_id);
    }
}

void Portfolio::place_market_order(const string &asset_id_, double units_,
                                const string &strategy_id_,
                                OrderExecutionType order_execution_type,
                                int trade_id)
{   
    auto asset_rp = this->exchange_map->asset_map.at(asset_id_);
  
    // build new smart pointer to shared order
    auto market_order = make_shared<Order>(MARKET_ORDER,
                                           asset_id_,
                                           units_,
                                           asset_rp->exchange_id,
                                           asset_rp->broker_id,
                                           this,
                                           strategy_id_,
                                           trade_id);

    if(this->event_tracer)
    {
        this->event_tracer->remember_order(market_order);
    }
    
    auto broker = this->brokers->at(asset_rp->broker_id);

    #ifdef ARGUS_STRIP
    if(this->logging){
        this->log_order_create(market_order);
    }
    #endif

    if (order_execution_type == EAGER)
    {
        // place order directly and process
        broker->place_order(market_order);
    }
    else
    {
        // push the order to the buffer that will be processed when the buffer is flushed
        broker->place_order_buffer(market_order);
    }
}

void Portfolio::place_limit_order(const string &asset_id_, double units_, double limit_,
                               const string &strategy_id_,
                               OrderExecutionType order_execution_type,
                               int trade_id)
{       

    auto asset_rp = this->exchange_map->asset_map.at(asset_id_);

    // build new smart pointer to shared order
    auto limit_order = make_shared<Order>(LIMIT_ORDER,
                                          asset_id_,
                                          units_,
                                          asset_rp->exchange_id,
                                          asset_rp->broker_id,
                                          this,
                                          strategy_id_,
                                          trade_id);

    if(this->event_tracer)
    {
        this->event_tracer->remember_order(limit_order);
    }

    // set the limit of the order
    limit_order->set_limit(limit_);

    auto broker = this->brokers->at(asset_rp->broker_id);

    if (order_execution_type == EAGER)
    {
        // place order directly and process
        broker->place_order(limit_order);
    }
    else
    {
        // push the order to the buffer that will be processed when the buffer is flushed
        broker->place_order_buffer(limit_order);
    }
}

void Portfolio::py_close_position(const string& asset_id)
{
    // close specific positino
    if(asset_id != "")
    {
        auto orders_nullopt = this->generate_order_inverse(asset_id, false, true);
    }
    //close all positions
    else
    {
        for(auto& position_pair : this->positions_map)
        {
            auto orders_nullopt = this->generate_order_inverse(position_pair.first, false, true);
        }
    }
}

void Portfolio::on_order_fill(order_sp_t filled_order)
{
    // log the order if needed
    #ifdef ARGUS_STRIP
    if (this->logging > 0)
    {
        this->log_order_fill(filled_order);
    }
    #endif

    // no position exists in the portfolio with the filled order's asset_id
    if (!this->position_exists(filled_order->get_asset_id()))
    {   
        this->open_position(filled_order, true);
        
        // set the trade's source portfolio
        auto position_sp = this->positions_map.at(filled_order->get_asset_id());
        auto trade_sp = position_sp->get_trades().begin()->second;
        trade_sp->set_source_position(position_sp.get());
    }
    else
    {
        auto position = this->get_position(filled_order->get_asset_id()).value();
        auto position_units = position->get_units();
        auto order_units = filled_order->get_units();

        // changing position sides
        if(position_units * order_units < 0 && abs(order_units) > abs(position_units))
        {
            // new order created to close out existing position, adjust the filled order so that it 
            // hold units need to open the position in the other direction
            auto new_order = split_order(
                filled_order,  
                -1 * position_units);

            // process adjusted orders, first closes out, second creates new position
            this->on_order_fill(new_order);
            this->on_order_fill(filled_order);

            // reset filled order units to the original value
            filled_order->set_units(order_units);
            return;
        }
        // filled order is not closing existing position
        else if (position_units + filled_order->get_units() > 1e-7)
        {
            this->modify_position(filled_order);
        }

        // filled order is closing an existing position
        else
        {
            /// close the position
            this->close_position(filled_order);
        }
    }

    // place child orders from the filled order
    for (auto &child_order : filled_order->get_child_orders())
    {
        auto broker = this->brokers->at(child_order->get_broker_id());
        broker->place_order(child_order);
    }
};

void Portfolio::modify_position(shared_ptr<Order> filled_order)
{
    // get the position and account to modify
    auto asset_id = filled_order->get_asset_id();
    auto position = this->get_position(asset_id).value();

    // adjust position and close out trade if needed
    auto trade = position->adjust_order(filled_order, this);

    //test to see if trade was closed
    if (!trade->get_is_open())
    {
        // cancel any orders linked to the closed traded
        this->trade_cancel_order(trade);

        //propgate trade close up portfolio tree
        auto portfolio_source = trade->get_source_portfolio();

        //find the source portfolio of the trade then propgate up trade closing
        if(portfolio_source->get_portfolio_id() != this->portfolio_id){
            //get the portfolio source
            auto source_portfolio = trade->get_source_portfolio();            
            
            //make sure we found source
            assert(source_portfolio);   

            //propgate trade close up the portfolio tree
            source_portfolio->propogate_trade_close_up(trade, true);

            // propogate_trade_close_up does not adjust the source, need to adjust the source portfolio
            auto source_position = source_portfolio->get_position(trade->get_asset_id()).value();
            source_position->adjust_trade(trade);
            if(!source_position->is_open)
            {
                source_portfolio->positions_map.erase(trade->get_asset_id());
                if(source_portfolio->event_tracer){source_portfolio->event_tracer->remember_position(position);}
            }
        }
        else{
            this->propogate_trade_close_up(trade, true);
        }

        // remember the trade
        if(this->event_tracer)
        {
            this->event_tracer->remember_trade(std::move(trade));
        }
    }
    //new trade
    else if (trade->get_trade_open_time() == filled_order->get_fill_time()){
        // set the trade's source portfolio
        trade->set_source_position(position.get());

        // propogate new trade up portfolio tree
        this->propogate_trade_open_up(trade, true);
    }

    // get fill info
    auto order_units = filled_order->get_units();
    auto order_fill_price = filled_order->get_average_price();

    // adjust cash for modifying position
    auto amount = gmp_mult(order_units, order_fill_price);
    gmp_sub_assign(this->cash, amount);

    auto diff = gmp_sub(order_fill_price, position->average_price);
    amount = gmp_mult(diff, order_units);

    this->nlv_adjust(amount);
}

void Portfolio::close_position(shared_ptr<Order> filled_order)
{
    // get the position to close and close it 
    auto asset_id = filled_order->get_asset_id();
    auto position = this->get_position(asset_id).value();

    #ifdef ARGUS_RUNTIME_ASSERT
    assert(position->get_units() + filled_order->get_units() < 1e-7);
    #endif 

    position->close(
        filled_order->get_average_price(), 
        filled_order->get_fill_time());

    #ifdef ARGUS_STRIP
    if(this->logging  > 0){
        this->log_position_close(position);
    }
    #endif

    // adjust cash held at the broker
    // sell order has units -XYZ, therefore need -=
    auto amount = gmp_mult(filled_order->get_units(), filled_order->get_average_price());
    gmp_sub_assign(this->cash, amount);

    // close all child trade of the position whose broker is equal to current broker id
    auto trades = position->get_trades();
    for (auto it = trades.begin(); it != trades.end(); it++)
    {
        auto trade = it->second;

        //cancel any open orders for the trade
        this->trade_cancel_order(trade);
        
        //find the source portfolio of the trade then propgate up trade closing
        auto portfolio_source = trade->get_source_portfolio();
        if(portfolio_source->get_portfolio_id() != this->portfolio_id){
            //get source portfolio for trade
            auto source_portfolio = trade->get_source_portfolio();

            //make sure we found source
            assert(source_portfolio);   

            //propgate  trade close up the portfolio tree
            source_portfolio->propogate_trade_close_up(trade, true);

            // propogate_trade_close_up does not adjust the source, need to adjust the source portfolio
            auto source_position = source_portfolio->get_position(trade->get_asset_id()).value();
            source_position->adjust_trade(trade);
            if(!source_position->is_open)
            {
                source_portfolio->positions_map.erase(trade->get_asset_id());
                if(source_portfolio->event_tracer){source_portfolio->event_tracer->remember_position(position);}
            }
        }

        //this is the source portfolio of the trade
        else{
            this->propogate_trade_close_up(trade, true);
        }

        #ifdef ARGUS_STRIP
        if(this->logging > 0){
            this->log_trade_close(trade);
        }
        #endif

        // push the trade to history, at this point local trade variable should have use count 1
        // history will validate it when it attempts to push to trade history
        if(this->event_tracer)
        {
            this->event_tracer->remember_trade(std::move(trade));
        }
    }

    //remove trades from position
    position->get_trades().clear();

    // remove the position from portfolio
    this->positions_map.erase(asset_id);

    // push position to history
    position->set_is_open(false);
    if(this->event_tracer)
    {
        this->event_tracer->remember_position(std::move(position));
    }
}

void Portfolio::propogate_trade_close_up(trade_sp_t trade_sp, bool adjust_cash){  
    if(!this->parent_portfolio){
        return;
    }
    auto parent = this->parent_portfolio;
    
    #ifdef ARGUS_RUNTIME_ASSERT
    auto parent_position = parent->get_position(trade_sp->get_asset_id());
    assert(parent_position.has_value());
    #endif
    
    //get the parent position
    auto position = parent->get_position(trade_sp->get_asset_id()).value();
    position->adjust_trade(trade_sp);

    //adjust parent portfolio cash
    auto cash_adjustment = gmp_mult(trade_sp->get_units(), trade_sp->get_close_price());
    parent->cash_adjust(cash_adjustment);

    //test to see if position should be removed
    if(position->get_trade_count() == 0)
    {   
        // log position closed by trade propogating up
        #ifdef ARGUS_STRIP
        if(this->logging > 0)
        {
            parent->log_position_close(position);
            
        }
        #endif

        // remove position from parent portfolio if there are no more trades
        parent->positions_map.erase(trade_sp->get_asset_id());

        // remember the postiion of the parent
        if(this->event_tracer)
        {
            this->event_tracer->remember_position(std::move(position));
        }
    }

    //recursively proprate trade up up portfolio tree
    if(parent->parent_portfolio)
    {
        parent->parent_portfolio->propogate_trade_close_up(trade_sp, adjust_cash);
    }
}

void Portfolio::propogate_trade_open_up(trade_sp_t trade_sp, bool adjust_cash){
    //reached master portfolio
    if(!this->parent_portfolio){
        return;
    }

    auto parent = this->parent_portfolio;
    
    //position does not exist in portfolio
    if(!parent->position_exists(trade_sp->get_asset_id()))
    {
            parent->open_position(trade_sp, adjust_cash);
    }
    //position already exists, add the new trade
    else
    {
        auto position = *parent->get_position(trade_sp->get_asset_id());
        //insert the trade into the position
        position->adjust_trade(trade_sp);

        //adjust parent portfolios cash
        auto cash_adjustment = -1 * gmp_mult(trade_sp->get_units(),trade_sp->get_average_price());
        parent->cash_adjust(cash_adjustment);

        //log the new trade open for the parent
        #ifdef ARGUS_STRIP
        if(this->logging > 0)
        {
            parent->log_trade_open(trade_sp);
        }
        #endif
    }
    
    //recursively proprate trade up up portfolio tree
    if(parent->parent_portfolio)
    {
        parent->parent_portfolio->propogate_trade_open_up(trade_sp, adjust_cash);
    }
};

shared_ptr<Portfolio> Portfolio::create_sub_portfolio(const string& portfolio_id_, double cash_){
    //create new portfolio
    auto portfolio_ = std::make_shared<Portfolio>(
        this->logging, 
        cash_, 
        portfolio_id_,
        this,
        this->brokers,
        this->exchange_map
        );
    
    //insert into child portfolio map
    this->portfolio_map.insert({portfolio_id, portfolio_});

    //update parent portfolio's values
    this->add_cash(portfolio_->get_cash());

    //return smart pointer to the new portfolio
    return portfolio_;
}

void Portfolio::add_sub_portfolio(const string &portfolio_id_, portfolio_sp_t portfolio_)
{
    auto iter = this->portfolio_map.find(portfolio_id_);
    if (this->portfolio_map.end() != iter)
    {
        ARGUS_RUNTIME_ERROR("Portfolio::add_sub_portfolio portfolio already exists");
    }
    this->portfolio_map.insert({portfolio_id, portfolio_});

    //make sure the parent portfolio of the passed portfolio is equal to this
    assert(this == portfolio_->get_parent_portfolio());

    //update parent portfolio's values
    this->add_cash(portfolio_->get_cash());

    //propgate all open positions and trade up the portfolio tree
    //done adjust cash, all parent portfolios simply take on child trades, i.e. NLV increases
    for(const auto& postion_pair : portfolio_->positions_map){
        auto position = postion_pair.second;
        
        //loop over all trades in the position
        for(const auto &trade_pair : position->get_trades()){
            auto trade = trade_pair.second;
            
            //propogate trade up portfolio tree
            portfolio_->propogate_trade_open_up(trade, false);
        }
    }
}

void Portfolio::update(){
    // save current portfolio state to the history
    this->portfolio_history->update();

    // recursively save states of all child portfolios
    for(auto &portfolio_pair : this->portfolio_map){
        portfolio_pair.second->update();
    }
}

optional<portfolio_sp_t> Portfolio::get_sub_portfolio(const string &portfolio_id_)
{
    auto iter = this->portfolio_map.find(portfolio_id_);
    if (this->portfolio_map.end() == iter)
    {
        return nullopt;
    }
    return this->portfolio_map.at(portfolio_id_);
}

void Portfolio::evaluate(bool on_close)
{
    #ifdef ARGUS_RUNTIME_ASSERT
    //for performance sake only run from master portfolio
    assert(!this->parent_portfolio);
    #endif

    this->nlv = this->cash;
    this->unrealized_pl = 0;

    // evaluate all positions in the master portfolio. Note valuation will propogate down from whichever
    // portfolio it was called on, i.e. all trades in child portfolios will be evaluated already
    for(auto it = this->positions_map.begin(); it != positions_map.end(); ++it) 
    {
        auto position = it->second;

        // get the exchange the asset is listed on
        auto exchange_id = position->get_exchange_id();
        auto market_price = this->exchange_map->get_market_price(it->first);

        // asset is not in market view
        if (market_price == 0)
        {
            continue;
        }

        auto trades = position->get_trades();
        //evaluate indivual trades, adjusting source portfolios as we go
        for(auto& trade_pair : trades){
            auto trade = trade_pair.second;

            //update source portfolio values nlv and unrealized pl
            auto source_portfolio = trade->get_source_portfolio();
            auto source_position = trade->get_source_position();

            // if the source is the master portfolio don't need to manually adjust
            if(!source_portfolio->get_parent_portfolio())
            {
                continue;
            }

            /// use gmp to get precesion adjustment
            auto nlv_new = gmp_mult(trade->get_units(), market_price);
            source_portfolio->nlv_adjust(
                gmp_sub(nlv_new , trade->get_nlv())
            );
            source_position->nlv_adjust(
                gmp_sub(nlv_new , trade->get_nlv())
            );    

            auto unrealized_pl_new = trade->get_units() * (market_price - trade->get_average_price());
            source_portfolio->unrealized_adjust(unrealized_pl_new - trade->get_unrealized_pl());
            source_position->unrealized_adjust(unrealized_pl_new - trade->get_unrealized_pl());

            //update trade values to new evaluations
            trade->set_unrealized_pl(unrealized_pl_new);
            trade->set_nlv(nlv_new);
            trade->set_last_price(market_price);

            if(on_close)
            {
                trade->bars_held++;
            }
        }

        position->evaluate(market_price, on_close);
        gmp_add_assign(this->nlv, position->get_nlv());
        this->unrealized_pl += position->get_unrealized_pl();
    }
}

void Portfolio::position_cancel_order(Broker::position_sp_t position_sp)
{
    auto trades = position_sp->get_trades();
    for (auto it = trades.begin(); it != trades.end();)
    {
        auto trade = it->second;
        // cancel orders whose parent is the closed trade
        for (auto &order : trade->get_open_orders())
        {
            auto broker = this->brokers->at(order->get_broker_id());
            broker->cancel_order(order->get_order_id());
        }
    }
}

optional<vector<order_sp_t>> Portfolio::generate_order_inverse( 
        const string & asset_id,
        bool send_orders,
        bool send_collapse)
{
    //get position to close
    auto position = this->get_position(asset_id);

    //make sure we found a position
    if(!position.has_value()){
        ARGUS_RUNTIME_ERROR("failed to find position");
    }

    //populate inverse orders container
    std::vector<order_sp_t> orders;
    position.value()->generate_order_inverse(orders);

    if(send_collapse){
        //genrate consolidated order
        auto orders_consolidated = OrderConsolidated(orders, this);

        //send order to broker
        auto broker_id = orders[0]->get_broker_id();
        auto broker = this->brokers->at(broker_id);
        auto parent_order = orders_consolidated.get_parent_order();

        broker->place_order(parent_order, false);

        //make sure the order was filled
        assert(parent_order->get_order_state() == FILLED);
        
        //fill child orders using parent fill
        orders_consolidated.fill_child_orders();
        auto child_orders = orders_consolidated.get_child_orders();

        for(auto child_order : child_orders){
            //process the orders indivually
            broker->process_filled_order(child_order);
        }

        return std::nullopt;
    }
    else if (send_orders){
        for(auto& order : orders){
            auto broker_id = order->get_broker_id();
            auto broker = this->brokers->at(broker_id);
            broker->place_order(order);

            //make sure the order was filled
            assert(order->get_order_state() == FILLED);

            //remember the order
            if(this->event_tracer)
            {
                this->event_tracer->remember_order(std::move(order));
            }
            return std::nullopt;
        }
    }
    return orders;
}

void Portfolio::trade_cancel_order(Broker::trade_sp_t &trade_sp)
{    
    for (auto &order : trade_sp->get_open_orders())
    {   
        // get corresponding broker for the order then cancel it
        auto broker = this->brokers->at(order->get_broker_id());
        broker->cancel_order(order->get_order_id());
    }
}

shared_ptr<Portfolio> Portfolio::find_portfolio(const string &portfolio_id_){
    if(this->portfolio_id == portfolio_id_){
        return shared_from_this();
    }
    
    //portfolio does not exist in sub portfolios, recurisvely search child portfolios
    for(auto& portfolio_pair : this->portfolio_map){
        auto portfolio = portfolio_pair.second;
        if (auto found = portfolio->find_portfolio(portfolio_id_)) 
        {
            return found;
        }
    }
    ARGUS_RUNTIME_ERROR("failed to find portfolio");
};

void Portfolio::add_cash(double cash_)
{
    this->cash += cash_;
    if(!this->is_built)
    {
        this->starting_cash += cash_;
    }
    if(!this->parent_portfolio)
    {
        return;
    }
    this->parent_portfolio->add_cash(cash_);
}

void Portfolio::consolidate_order_history(vector<shared_ptr<Order>>& orders)
{
    // search through all portfolios for order histories
    for(auto& portfolio_pair : this->portfolio_map)
    {
        portfolio_pair.second->consolidate_order_history(orders);
    }

    // get order tracer if exists
    auto tracer = this->portfolio_history->get_tracer(Event);
    if(!tracer)
    {
        return;
    }

    // copy sp to orders into the vector
    auto event_tracer = static_cast<EventTracer*>(tracer.get());
    int i = 0;
    for(auto& order : event_tracer->get_order_history())
    {
        orders.push_back(order);
        i ++;
    }
}

#ifdef ARGUS_STRIP
void Portfolio::log_position_open(shared_ptr<Position> &new_position)
{
    auto datetime_str = nanosecond_epoch_time_to_string(new_position->get_position_open_time());
    fmt::print("{}:  PORTFOLIO {} NEW POSITION: POSITION {}, ASSET_ID: {}, AVG PRICE AT {:.3f}, UNITS: {:.3f}\n",
                datetime_str,
                this->portfolio_id,
                new_position->get_position_id(),
                new_position->get_asset_id(),
                new_position->get_average_price(),
                new_position->get_units());
}

void Portfolio::log_position_close(shared_ptr<Position> &new_position)
{
    auto datetime_str = nanosecond_epoch_time_to_string(new_position->get_position_close_time());
    fmt::print("{}:  PORTFOLIO {} CLOSED POSITION: POSITION {}, ASSET_ID: {}, CLOSE PRICE AT {:.3f}, UNITS: {:.3f}\n",
                datetime_str,
                this->portfolio_id,
                new_position->get_position_id(),
                new_position->get_asset_id(),
                new_position->get_close_price(),
                new_position->get_units());
}

void Portfolio::log_trade_close(shared_ptr<Trade> &closed_trade)
{
    auto datetime_str = nanosecond_epoch_time_to_string(closed_trade->get_trade_close_time());
    fmt::print("{}:  PORTFOLIO {} CLOSED TRADE: TRADE {} CLOSE PRICE AT {:.3f}, ASSET_ID: {}\n",
               datetime_str,
               this->portfolio_id,
               closed_trade->get_trade_id(),
               closed_trade->get_close_price(),
               closed_trade->get_asset_id());
}

void Portfolio::log_trade_open(trade_sp_t &new_trade)
{   
    auto datetime_str = nanosecond_epoch_time_to_string(new_trade->get_trade_open_time());
    fmt::print("{}:  PORTFOLIO {} TRADE OPENED: source portfolio id: {}, trade id: {}, asset id: {}, avg price: {:.3f}\n",
               datetime_str,
               this->portfolio_id,
               new_trade->get_source_portfolio()->get_portfolio_id(),
               new_trade->get_trade_id(),
               new_trade->get_asset_id(),
               new_trade->get_average_price());
};

void Portfolio::log_order_create(order_sp_t &filled_order)
{
    auto datetime_str = nanosecond_epoch_time_to_string(filled_order->get_order_create_time());
    fmt::print("{}:  PORTFOLIO {} ORDER CREATED: order id:  {}, asset id: {}, units: {:.3f}, trade id: {}\n",
               datetime_str,
               this->portfolio_id,
               filled_order->get_order_id(),
               filled_order->get_asset_id(),
               filled_order->get_units(),
               filled_order->get_trade_id());
};

void Portfolio::log_order_fill(order_sp_t &filled_order)
{
    auto datetime_str = nanosecond_epoch_time_to_string(filled_order->get_fill_time());
    fmt::print("{}:  PORTFOLIO {} ORDER FILLED: order id: {}, asset id: {}, avg price: {:.3f}, units: {:.3f}\n",
                datetime_str,
                this->portfolio_id,
                filled_order->get_order_id(),
                filled_order->get_asset_id(),
                filled_order->get_average_price(),
                filled_order->get_units());
};

PortfolioHistory::PortfolioHistory(Portfolio* parent_portfolio_): parent_portfolio(parent_portfolio_){
    //default ad values tracer, change to optional latter
    this->tracers.push_back(std::make_shared<ValueTracer>(parent_portfolio_));
};


#endif
