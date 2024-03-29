//
// Created by Nathan Tormaschy on 4/21/23.
//
#include "order.h"

#include "settings.h"
#include "utils_array.h"
#include <cstddef>
#include <cstdio>
#include <memory>

using order_sp_t = Order::order_sp_t;

order_sp_t split_order(order_sp_t existing_order, double new_order_units){
    // make deep copy of existing order
    auto new_order = std::make_shared<Order>(*existing_order);

    // subtract new order units from existing order
    auto existing_units = existing_order->get_units() - new_order_units;

    // update order units for both new and existing order
    new_order->set_units(new_order_units);
    existing_order->set_units(existing_units);

    // return sp to new order created
    return new_order;
}

OrderConsolidated::OrderConsolidated(vector<shared_ptr<Order>> orders, Portfolio* source_portfolio){
    double units_ = 0;

    //validate at least 1 order
    assert(orders.size() > 0);

    //get the first asset id (all must match)
    auto order = orders[0];
    auto asset_id_ = order->get_asset_id();
    auto broker_id_ = order->get_broker_id();

    for(auto const &order : orders){
        units_ += order->get_units();

        #ifdef ARGUS_RUNTIME_ASSERT
        //orders must have same asset id
        assert(order->get_asset_id() == asset_id_);

        //orders must have same broker id
        assert(order->get_broker_id() == broker_id_);
        
        //orders must be market orders
        assert(order->get_order_type() == MARKET_ORDER);
        #endif
    }
    
    this->parent_order = make_shared<Order>(MARKET_ORDER,
            asset_id_,
            units_,
            order->get_exchange_id(),
            broker_id_,
            source_portfolio,
            "master",
            0);

    this->child_orders = std::move(orders);
}

void OrderConsolidated::fill_child_orders(){
    #ifdef ARGUS_RUNTIME_ASSERT
    //validate parent order was filled
    assert(this->parent_order->get_order_state() == FILLED);
    #endif

    auto market_price = parent_order->get_average_price();
    auto fill_time = parent_order->get_fill_time();

    for(auto& child_order : this->child_orders){
        child_order->fill(market_price, fill_time);
    }
}

Order::Order(OrderType order_type_, string asset_id_, double units_, string exchange_id_,
             string broker_id_, Portfolio* source_portfolio, string strategy_id_, int trade_id_)
{
    this->order_type = order_type_;
    this->units = units_;
    this->average_price = 0.0;
    this->order_fill_time = 0;

    // populate the ids of the order
    this->asset_id = asset_id_;
    this->exchange_id = exchange_id_;
    this->broker_id = broker_id_;
    this->strategy_id = strategy_id_;

    // verify we have a valid source portfolio
    assert(source_portfolio);    
    this->source_portfolio = source_portfolio;

    // set the order id to 0 (it will be populated by the broker object who sent it
    this->order_id = this->order_counter;
    this->order_counter++;

    // set the limit to 0, broker will populate of the order type is tp or sl
    this->limit = 0;

    // set the trade id (-1 default value implies use the default trade of a position
    this->trade_id = trade_id_;

    // set the order state equal to PENDING (yet to be place)
    this->order_state = PENDING;

    // set the order parent to nullptr at first
    this->order_parent = nullptr;
}

void Order::fill(double market_price, long long fill_time)
{
    this->average_price = market_price;
    this->order_fill_time = fill_time;
    this->order_state = FILLED;
}

void Order::unfill()
{
    this->average_price = 0.0;
    this->order_fill_time = 0;
    this->order_state = PENDING;
}

void Order::cancel_child_order(size_t order_id_)
{
    auto _order = unsorted_vector_remove(
        this->child_orders,
        [](const shared_ptr<Order> &obj)
        { return obj->get_order_id(); },
        order_id_);
}

size_t Order::get_unsigned_trade_id() const
{
    auto trade_id_int = this->trade_id;
    if (trade_id_int == -1)
    {
        trade_id_int++;
    }
    auto trade_id_uint = static_cast<size_t>(trade_id_int);
    return trade_id_int;
};

OrderParent *Order::get_order_parent() const
{
#ifdef ARGUS_RUNTIME_ASSERT
    // asser that order parent is not nullptr
    assert(this->order_parent);
#endif
    return this->order_parent;
}
