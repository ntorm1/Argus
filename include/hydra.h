//
// Created by Nathan Tormaschy on 4/19/23.
//

#ifndef ARGUS_HYDRA_H
#define ARGUS_HYDRA_H

#include <functional>
#include "pch.h"

#include "asset.h"
#include "exchange.h"
#include "account.h"
#include "portfolio.h"
#include "broker.h"
#include "strategy.h"

using namespace std;

class Hydra
{
private:
    using portfolio_sp_t = Portfolio::portfolio_sp_t;
    using exchanges_sp_t = ExchangeMap::exchanges_sp_t;
    
    typedef shared_ptr<Brokers> brokers_sp_t;

    /// logging level
    int logging;

    /// is the hydra built
    bool is_built;

    /// mapping between broker id and portfolio held at broker
    portfolio_sp_t master_portfolio;

    /// current simulation time
    long long hydra_time;

    /// master datetime index of the combined exchanges
    long long *datetime_index{};

    /// current index of the datetime
    size_t current_index = 0;

    /// length of datetime index
    size_t datetime_index_length = 0;

    /// total number of rows in the hydra across all exchanges
    size_t candles = 0;

    // function calls on open
    vector<shared_ptr<Strategy>> strategies;

    void log(const string& msg);

public:
    using asset_sp_t = Asset::asset_sp_t;

    /// hydra constructor
    Hydra(int logging_, double cash = 0.0);

    /// hydra destructor
    ~Hydra();

    /// mapping between exchange id and smart pointer to an exchange
    exchanges_sp_t exchange_map;

    /// mapping between broker id and smart pointer to a broker
    brokers_sp_t brokers{};
    
    /// build all members
    void build();

    /// reset all members
    void reset(bool clear_history = true, bool clear_strategies = false);

    /// clear all existing strategies 
    void reset_strategies(){this->strategies.clear();}

    // process orders that were placed at the open
    void on_open();

    // forward pass of hydra
    void forward_pass();

    // backward pass of hydra
    void backward_pass();

    /**
     * @brief run the simulation
     * 
     * @param to run to this point in time. If not passed simulated to the end
     * @param steps number of steps to run to
     */
    void run(long long to = 0, size_t steps = 0);

    /**
     * @brief move simulation forward to an exact moment in the datetime indx
     * 
     * @param datetime ns epoch datetime to go to
     */
    void goto_datetime(long long datetime);

    /**
     * @brief replay the simulation using the historical order buffer
     * 
     */
    void replay();

    /**
     * @brief in a hydra replay, place all orders for the current hydra time
     * 
     * @param order_history        reference of vector of historical orders
     * @param on_close             replay is currently on close
     * @param current_order_index  the index if of the order we are currently on
     */
    void process_order_history(vector<shared_ptr<Order>>& order_history, bool on_close, size_t& current_order_index);

    /**
     * @brief get a consolidated vector of all orders placed from portfolio's with event tracers
     * 
     * @return vector<shared_ptr<Order>> vector of orders placed
     */
    vector<shared_ptr<Order>> get_order_history();
    
    /// @brief evaluate the portfolio at the current market prices
    void evaluate_portfolio(bool on_close);

    /// @brief get current simulation time
    long long get_hydra_time() {return this->hydra_time;}

    /// @brief get sp to master portfolio
    shared_ptr<Portfolio> get_master_portflio() {return this->master_portfolio;}

    /// @brief search for a portfolio in portfolio tree and return it 
    shared_ptr<Portfolio> get_portfolio(const string & portfolio_id);

    /// @brief get shared pointer to an exchange
    shared_ptr<Exchange> get_exchange(const string &exchange_id);

    /**
     * @brief Get an existing asset from the exchange map
     * 
     * @param asset_id      unique id of the asset to get
     * @return asset_sp_t   axisting asset 
     */
    optional<asset_sp_t> get_asset(const string& asset_id);

    /// @brief get shared pointer to a broker
    broker_sp_t get_broker(const string &broker_id);

    /// @brief add a new child portfolio to the master portfolio and return sp to it
    shared_ptr<Portfolio> new_portfolio(const string & portfolio_id, double cash);

    /// @brief add a new exchange to hydra class
    shared_ptr<Exchange> new_exchange(const string &exchange_id);

    /// @brief create new strategy class
    shared_ptr<Strategy> new_strategy(string strategy_id = "default", bool replace_if_exists = false);

    /// @brief remove a strategy class from the vector of registered strategies
    void remove_strategy(string strategy_id);

    /// @brief add a new broker
    broker_sp_t new_broker(const string &broker_id, double cash);

    /// @brief total number of rows loaded
    size_t get_candles(){return this->candles;}

    /// @brief get numpy array read only view into the simulations's datetime index
    py::array_t<long long> get_datetime_index_view();
    
    /// @brief handle a asset id that has finished streaming (remove from portfolio and exchange)
    void cleanup_asset(const string& asset_id);

    /// @brief self to void ptr and return
    void* void_ptr() { return static_cast<void*>(this);};

    /**
     * @brief register a new asset to an exchange
     * 
     * @param asset         the new asset to be registered
     * @param exchange_id   the unique id of the exchange to register it to
     */
    void register_asset(const asset_sp_t &asset, const string & exchange_id);

    /**
     * @brief register a new index asset to an exhcnage or all exchanges
     * 
     * @param asset         index asset to register
     * @param exchange_id   unique id of the exchange to register to, if none passed then registers to all
     */
    void register_index_asset(const asset_sp_t &asset, string exchange_id = "");
    

};

/// function for creating a shared pointer to a hydra
shared_ptr<Hydra> new_hydra(int logging);

#endif // ARGUS_HYDRA_H
