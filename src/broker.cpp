//
// Created by Nathan Tormaschy on 4/21/23.
//
#include <cstdio>
#include <stdexcept>
#include "pch.h"
#include <fmt/core.h>


#include "broker.h"
#include "position.h"
#include "account.h"
#include "settings.h"
#include "utils_array.h"
#include "utils_time.h"

using namespace std;


Broker::Broker(string broker_id_, double cash_, int logging_) : broker_account(broker_id_, cash_)
{
    this->broker_id = std::move(broker_id_);
    this->cash = cash_;
    this->logging = logging_;
    this->com_scheme = nullopt;
}

void Broker::build(
    exchanges_sp_t exchange_map_)
{
    this->exchange_map = exchange_map_;
    this->starting_cash = cash;
}

void Broker::reset_broker()
{   
    //reset memeber variables
    this->cash = starting_cash;

    // clear order buffers
    this->open_orders.clear();
    this->open_orders_buffer.clear();

    //reset brokers account
    this->broker_account.reset();
}


void Broker::cancel_order(size_t order_id)
{
    auto order_opt = unsorted_vector_remove(
        this->open_orders,
        [](const shared_ptr<Order> &obj)
        { return obj->get_order_id(); },
        order_id);

    if(!order_opt.has_value())
    {
        throw std::runtime_error("failed to find order id to cancel");
    }

    //unwrap optional we know has value
    auto order = order_opt.value();
    
    // set the order state to cancel
    order->set_order_state(CANCELED);

    // get the order's parent if exists
    auto order_parent_struct = order->get_order_parent();

    // if the order has no parent then return
    if (!order_parent_struct)
    {
        // move canceled order to history
        return;
    }

    // remove the open order from the open order's parent
    switch (order_parent_struct->order_parent_type)
    {
        case TRADE:
        {
            auto trade = order_parent_struct->member.parent_trade;
            trade->cancel_child_order(order_id);
            break;
        }
        case ORDER:
        {
            auto parent_order = order_parent_struct->member.parent_order;
            parent_order->cancel_child_order(order_id);
            break;
        }
    }

    // recursively cancel all child orders of the canceled order
    for (auto &child_orders : order->get_child_orders())
    {
        this->cancel_order(child_orders->get_order_id());
    }
}

void Broker::place_order_buffer(shared_ptr<Order> order)
{
    this->open_orders_buffer.push_back(order);
}

void Broker::log_order_place(shared_ptr<Order> &filled_order)
{
    auto datetime_str = nanosecond_epoch_time_to_string(filled_order->get_order_create_time());
    fmt::print("{}:  BROKER {} ORDER PLACED: order id:  {}, asset id: {}, units: {:.3f}, trade id: {}\n",
               datetime_str,
               this->broker_id,
               filled_order->get_order_id(),
               filled_order->get_asset_id(),
               filled_order->get_units(),
               filled_order->get_trade_id());
};

void Broker::place_order(shared_ptr<Order> order, bool process_fill)
{
    // get smart pointer to the right exchange
    auto exchange = this->exchange_map->exchanges.at(order->get_exchange_id());

    // set wether the ored was placed on the close or open
    order->set_placed_on_close(exchange->on_close);

    // send the order
    exchange->place_order(order);

    if(this->logging)
    {
        this->log_order_place(order);
    }

    // if the order was filled then process fill
    if (order->get_order_state() == FILLED)
    {
        if(process_fill)
        {
            this->process_filled_order(std::move(order));
        }
    }
    // else push the order to the open order vector to be monitored
    else
    {
        this->open_orders.push_back(order);
    }
}

void Broker::send_orders()
{
    // send orders from buffer to the exchange
    for (auto &order : this->open_orders_buffer)
    {
        // get the exchange the order was placed to
        auto exchange = exchange_map->exchanges.at(order->get_exchange_id());

        // send order to rest on the exchange
        exchange->place_order(order);

        if(this->logging)
        {
            this->log_order_place(order);
        }

        if (order->get_order_state() == FILLED)
        {
            // process order that has been filled
            this->process_filled_order(order);
        }
        else
        {
            // add the order to current open orders
            this->open_orders.push_back(order);
        }
    }
    // clear the order buffer as all orders are now open
    this->open_orders_buffer.clear();
}

void Broker::process_filled_order(order_sp_t filled_order)
{
    #ifdef DEBUGGING
    printf("broker processing filled order...\n");
    #endif

    assert(filled_order->get_source_portfolio());
    
    // adjust the account held at the broker
    #ifdef ARGUS_BROKER_ACCOUNT_TRACKING
    auto new_order = std::make_shared<Order>(*filled_order);
    this->broker_account.on_order_fill(new_order);
    #endif

    // get the portfolio the order was placed for, adjust the sub portfolio accordingly
    auto portfilio = filled_order->get_source_portfolio();

    if(this->com_scheme.has_value())
    {
        double commision = 0.0f;
        commision += this->com_scheme.value().flat_com;
        commision += this->com_scheme.value().pct_com * abs(filled_order->get_units()) * filled_order->get_average_price();
        portfilio->add_cash(-1*commision);
        this->cash -= commision;
    }

    portfilio->on_order_fill(filled_order);

    #ifdef DEBUGGING
    printf("broker filled order processed \n");
    #endif
}

void Broker::process_orders()
{
    // iterate over open orders and process and filled ones
    for (auto it = this->open_orders.begin(); it != this->open_orders.end();)
    {
        auto order = *it;

        if (order->get_order_state() != FILLED)
        {
            it++;
            continue;
        }
        else
        {
            // process the individual order
            this->process_filled_order(std::move(order));

            // remove filled order and move to next open order
            it = this->open_orders.erase(it);
        }
    }
};
