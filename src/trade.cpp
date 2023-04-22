//
// Created by Nathan Tormaschy on 4/21/23.
//

#include <memory>
#include <string>

#include "trade.h"
#include "utils_array.h"

using namespace std;

shared_ptr<Order> Trade::cancel_child_order(unsigned int order_id) {
    auto order = unsorted_vector_remove(
            this->open_orders,
            [](const shared_ptr<Order> &obj) { return obj->get_order_id(); },
            order_id
    );
    return order;
}

Trade::Trade(unsigned int trade_id_, double units_, double average_price_, long long int trade_open_time_) {
    this->trade_id = trade_id_;

    this->units = units_;
    this->average_price = average_price_;
    this->unrealized_pl = 0;
    this->realized_pl = 0;
    this->close_price = 0;
    this->last_price = 0;


    this->trade_close_time = 0;
    this->trade_open_time = trade_open_time_;
    this->bars_held = 0;

}

void Trade::evaluate(double market_price, bool on_close) {
    this->last_price = market_price;
    this->unrealized_pl = this->units * (market_price - this->average_price);
    if(on_close){
        this->bars_held++;
    }
}