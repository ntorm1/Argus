//
// Created by Nathan Tormaschy on 4/21/23.
//
#include <string>
#include <memory>
#include <utility>
#include <fmt/core.h>

#include "broker.h"
#include "utils_array.h"
#include "utils_time.h"

using namespace std;

Broker::Broker(string broker_id_, double cash_, int logging_){
    this->broker_id = std::move(broker_id_);
    this->cash = cash_;
    this->logging = logging_;
}

shared_ptr<Order> Broker::cancel_order(unsigned int order_id) {
    auto order = unsorted_vector_remove(
            this->open_orders,
            [](const shared_ptr<Order> &obj) { return obj->get_order_id(); },
            order_id
    );

    //get the order's parent if exists
    auto order_parent_struct = order->get_order_parent();

    //if the order has no parent then return
    if(!order_parent_struct){
        return order;
    }

    //remove the open order from the open order's parent
    switch (order_parent_struct->order_parent_type) {
        case TRADE: {
            auto trade = order_parent_struct->member.parent_trade;
            auto child_order = trade->cancel_child_order(order_id);
        }
        case ORDER: {
            auto parent_order = order_parent_struct->member.parent_order;
            auto child_order = parent_order->cancel_child_order(order_id);
        }
    }

    //recursively cancel all child orders of the canceled order
    for(auto& child_orders : order->get_child_orders()){
        this->cancel_order(child_orders->get_order_id());
    }

    return order;
}

void Broker::place_market_order(const string& asset_id_, double units_,
                        const string& exchange_id_,
                        const string& account_id_,
                        const string& strategy_id_){
    //build new smart pointer to shared order
    auto market_order =  make_shared<Order>
            (MARKET_ORDER,
             asset_id_,
             units_,
             exchange_id_,
             this->broker_id,
             account_id_,
             strategy_id_);

    //push the order to the buffer that will be processed when the buffer is flushed
    this->open_orders_buffer.push_back(market_order);
}

void Broker::place_limit_order(const string& asset_id_, double units_, double limit_,
                                const string& exchange_id_,
                                const string& account_id_,
                                const string& strategy_id_){
    //build new smart pointer to shared order
    auto limit_order =  make_shared<Order>
            (LIMIT_ORDER,
             asset_id_,
             units_,
             exchange_id_,
             this->broker_id,
             account_id_,
             strategy_id_);

    //set the limit of the order
    limit_order->set_limit(limit_);

    //push the order to the buffer that will be processed when the buffer is flushed
    this->open_orders_buffer.push_back(limit_order);
}

void Broker::send_orders( tsl::robin_map<string,shared_ptr<Exchange>>& exchanges){
    //send orders from buffer to the exchange
    for(auto & order : this->open_orders_buffer){
        //get the exchange the order was placed to
        auto exchange = exchanges.at(order->get_exchange_id());

        //send order to rest on the exchange
        exchange->place_order(order);

        //set the order to state to open as it is now open on exchange
        order->set_order_state(OPEN);

        //add the order to current open orders
        this->open_orders.push_back(order);
    }

    //clear the order buffer as all orders are now open
    this->open_orders_buffer.clear();
}


void Broker::log_order_fill(shared_ptr<Order>& filled_order){
    auto datetime_str = nanosecond_epoch_time_to_string(filled_order->get_fill_time());
    fmt::print("{}:  BROKER {}: ORDER {} FILLED AT {}, ASSET_ID: {}",
               datetime_str,
               broker_id,
               filled_order->get_order_id(),
               filled_order->get_fill_price(),
               filled_order->get_asset_id());
};

void Broker::process_orders(){
    vector<size_t> filled_order_indecies;
    size_t index = 0;
    for(auto& open_order : this->open_orders){
        if(open_order->get_order_state() == FILLED){

        }
    }
    //remove filled orders
    for(auto order_index : filled_order_indecies){
        //pop filled order off of the back
        std::swap(this->open_orders[order_index], this->open_orders.back());
        auto filled_order = this->open_orders.back();
        this->open_orders.pop_back();

        //log the order if needed
        if(this->logging == 1){
            this->log_order_fill(filled_order);
        }

        //push the filled order to the history
        historical_orders.push_back(filled_order);
    }
};
