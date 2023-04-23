//
// Created by Nathan Tormaschy on 4/21/23.
//
#pragma once
#ifndef ARGUS_TRADE_H
#define ARGUS_TRADE_H
#include <string>
#include <vector>
#include <memory>

#include "order.h"

using namespace std;

class Trade
{

public:
    typedef shared_ptr<Trade> trade_sp_t;
    /// trade constructor
    Trade(shared_ptr<Order> &filled_order, unsigned int trade_id_);

    void adjust(shared_ptr<Order> &filled_order);

    /// close the trade out at given time and price
    /// \param market_price price the trade was closed out at
    /// \param trade_close_time time the trade was closed out at
    void close(double market_price, long long trade_close_time);

    /// reduce trade side at given time and price
    /// \param market_price market price the adjustment order was filled at
    /// \param units number of units to reduce
    /// \param trade_change_time time the trade was changed
    void reduce(double market_price, double units, long long trade_change_time);

    /// increase trade side at given time and price
    /// \param market_price market price the adjustment order was filled at
    /// \param units number of units increase
    /// \param trade_change_time time the trade was changed
    void increase(double market_price, double units, long long trade_change_time);

    /// remove open order that has been canceled;
    void cancel_child_order(unsigned int order_id);

    /// is the trade currently open
    /// \return is the trade open
    [[nodiscard]] bool get_is_open() const { return this->is_open; }

    /// get number of units in the trade
    /// \return number of units in the trade
    [[nodiscard]] double get_units() const { return this->units; }

    /// get closing price of the trade
    /// \return closing price of the trade
    [[nodiscard]] double get_close_price() const { return this->close_price; }

    /// get the id of the underlying asset of the trade
    /// \return underlying asset of the trade
    [[nodiscard]] string get_asset_id() const { return this->asset_id; }

    /// get the average price of the trade
    /// \return average price of the trade
    [[nodiscard]] double get_average_price() const { return this->average_price; }

    /// get the realized pl of the trade
    /// \return realized pl of the trade
    [[nodiscard]] double get_realized_pl() const { return this->realized_pl; }

    /// @brief get all open orders placed at the broker
    /// @return ref to vec of order smart pointers
    vector<shared_ptr<Order>> &get_open_orders() { return this->open_orders; }

    /// evaluate the trade at the current market price either at open of close of candle
    /// \param market_price current market price of the underlying asset
    /// \param on_close is it the open or close of current candle
    void evaluate(double market_price, bool on_close);

private:
    /// is the trade currently open
    bool is_open;

    /// unique id of the trade
    unsigned int trade_id;

    /// unique id of the underlying asset of the trade
    string asset_id;

    /// unique id of the exchange the underlying asset is on
    string exchange_id;

    /// unique id of the broker the trade was placed on
    string broker_id;

    /// unique id of the account the trade belongs to
    string portfolio_id;

    /// how many units in the trade
    double units;

    /// average price of the trade
    double average_price;

    /// closing price of the trade
    double close_price;

    /// last price the trade was evaluated at
    double last_price;

    /// unrealized pl of the trade
    double unrealized_pl;

    /// realized pl of the trade
    double realized_pl;

    /// time the trade was opened
    long long trade_open_time;

    /// time the trade was closed
    long long trade_close_time;

    /// time the trade was changed
    long long trade_change_time;

    /// number of bars the positions has been held for
    unsigned int bars_held;

    /// child orders currently open, used for take profit and stop loss orders
    vector<shared_ptr<Order>> open_orders;
};

#endif // ARGUS_TRADE_H
