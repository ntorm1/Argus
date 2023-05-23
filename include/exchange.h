//
// Created by Nathan Tormaschy on 4/18/23.
//

#ifndef ARGUS_EXCHANGE_H
#define ARGUS_EXCHANGE_H
#include "pch.h"

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>

#include "asset.h"
#include "order.h"

#include "pybind11/pytypes.h"
#include "utils_array.h"

using namespace std;
namespace py = pybind11;

enum ExchangeQueryType
{
    Default,
    NLargest,
    NSmallest,
    NExtreme
};

class ExchangeMap;

class Exchange
{
friend class ExchangeMap;  
public:
    typedef shared_ptr<Exchange> exchange_sp_t;

    using asset_sp_t = Asset::asset_sp_t;

    /// exchange constructor
    Exchange(string exchange_id_, int logging_);

    /// destructor for the exchange
    ~Exchange();

    /// Disable move constructor
    Exchange(Exchange &&) = delete;

    /// Disable move assignment operator
    Exchange &operator=(Exchange &&) = delete;

    /// map between asset id and asset pointer
    std::unordered_map<string, asset_sp_t> market;

    /// total number of rows in the exhange
    size_t candles;

    /// is the close of a candle
    bool on_close;

    /// build the exchange
    void build();

    /// reset the exchange to the start of the simulation
    void reset_exchange();

    /// build the market view, return false if all assets listed are done streaming
    bool get_market_view();

    /**
     * @brief register a new index asset to the exchange. Index asset must have the same datetime
     *  index as the exchange and is used to set the beta of indivual asset's listed on the exchange.
     *  Once registered, the exchange will link it to each asset currently registered on the exchange
     * 
     * @param index an asset object to register as the exchange's index
     */
    void register_index_asset(const asset_sp_t &index);

    optional<asset_sp_t> get_index_asset();

    /**
     * @brief move assets that have expired in the last time step out of the market and market view
     * 
     */
    void move_expired_assets();

    /**
     * @brief add a new tracer to all assets listed on the exchange
     * 
     * @param tracer_type       type of tracer to add
     * @param lookback          lookback of the tracer
     * @param adjust_warmup     wether to adjust the asset's warmup
     */
    void add_tracer(AssetTracerType tracer_type, size_t lookback, bool adjust_warmup);

    /**
     * @brief get a list of asset's that have expired in the current time step
     *  An asset expires when it reaches the end of it's datetime index
     * @return optional<vector<asset_sp_t>*> optional pointer to vector of assets, None if no assets expired.
     */
    optional<vector<asset_sp_t>*> get_expired_assets();

    /// process open orders on the exchange
    void process_orders();

    /// place order to the exchange
    void place_order(shared_ptr<Order> &order);

    /// set wether or not currently at close or open of time step
    void set_on_close(bool on_close_) { this->on_close = on_close_; }

    /// build a new asset on the exchange
    asset_sp_t new_asset(const string &asset_id, const string& broker_id);

    /// get smart pointer to existing asset on the exchange
    asset_sp_t get_asset(const string &asset_id) const;

    /// get numpy array read only view into the exchange's datetime index
    py::array_t<long long> get_datetime_index_view();

    /// get read only pointer to datetime index
    long long const * get_datetime_index() { return this->datetime_index; }

    /// move exchange to specific point in time
    void goto_datetime(long long datetime);

    /// get read exchange current time
    long long get_datetime() { return this->datetime_index[this->current_index]; }

    /// is the exchange built yet
    [[nodiscard]] bool get_is_built() const { return this->is_built; }

    /// return the number of rows in the asset
    [[nodiscard]] size_t get_rows() const { return this->datetime_index_length; }

    /// get a values from asset data by column and row, (index 0 is current, row -1 is previous row)
    optional<double> get_asset_feature(const string& asset_id, const string& column, int index = 0);

    /**
     * @brief Get asset feature for every asset listed on the exchange. 
     *  - Every asset on the exchange must have the passed column
     *  - If a asset is not currently streaming it will be skipped
     * 
     * @param column the name of the column to query
     * @param row the index of the row to get, 0 is current, -1 is previous, etc.
     * @param query_type the type of the query, allows for efficent sorting and complex queries
     * @param N number of assets to query, defaults to -1 which returns all
     * @return py::dict 
     */
    py::dict get_exchange_feature(
        const string& column, 
        int row = 0, 
        ExchangeQueryType query_type   =  ExchangeQueryType::Default,
        optional<AssetTracerType> query_scaler = nullopt,
        int N = -1
    );

    inline double get_market_price(const string &asset_id)
    {
        // get pointer to asset, nullptr if asset is not currently streaming
        auto asset_raw_pointer = this->market_view.at(asset_id);
        if (asset_raw_pointer)
        {
            return asset_raw_pointer->get_market_price(this->on_close);
        }
        else
        {
            return 0.0;
        }
    }
private:
    /// logging level
    int logging;

    /// is the exchange built
    bool is_built;

    /// unique id of the exchange
    string exchange_id;

    /**
     * @brief a smart pointer to a asset representing the index of the exchange
     *  - index asset must have a datetime index equivalent to the exchange and can 
     *    only be registered once the exchange is built.
     */
    optional<asset_sp_t> index_asset = nullopt;

    /// mapping for asset's available at the current moment;
    std::unordered_map<string, Asset *> market_view;

    /// container for storing asset_id's that have finished streaming
    vector<asset_sp_t> expired_assets;

    /// open orders on the exchange
    vector<shared_ptr<Order>> open_orders;

    /// current exchange time
    long long exchange_time;

    /// exchange datetime index
    long long *datetime_index;

    /// length of datetime index
    size_t datetime_index_length;

    /// current position in datetime index
    size_t current_index;

    /**
     * @brief register a new asset on the exchange, only to be called through the friend ExchangeMap class
     * 
     * @param asset new asset to register
     */
    void register_asset(const asset_sp_t &asset);
    
    /// process open orders on the exchange
    void process_order(shared_ptr<Order> &open_order);

    /// process a market order currently open
    void process_market_order(shared_ptr<Order> &open_order);

    /// process a limit order currently open
    void process_limit_order(shared_ptr<Order> &open_order);

    /// process a stop loss order currently open
    void process_stop_loss_order(shared_ptr<Order> &open_order);

    /// process a take profit order currently open
    void process_take_profit_order(shared_ptr<Order> &open_order);
};

class ExchangeMap{
public:
    typedef std::unordered_map<string, shared_ptr<Exchange>> Exchanges;
    typedef shared_ptr<ExchangeMap> exchanges_sp_t;

    using asset_sp_t = Asset::asset_sp_t;
    using exchange_sp_t = Exchange::exchange_sp_t;

    /// mapping between exchange id and exchange object
    Exchanges exchanges;

    /// mapping between asset id and asset pointer
    std::unordered_map<string, asset_sp_t> asset_map;

    /// wether the exchanges are on the close step or open
    bool on_close = false;

    /**
     * @brief register a new asset to the exchange map
     * 
     * @param asset_  shared pointer to new asset to register
     * @param exchange_id unique id of the exchange to register it too
     */
    void register_asset(const shared_ptr<Asset>& asset_, const string& exchange_id_);

    /**
     * @brief get an existing asset on the exchange map
     * 
     * @param asset_id_                 unique id of the asset to look for
     * @return optional<asset_sp_t>     existing asset, none if not exists
     */
    optional<asset_sp_t> get_asset(const string& asset_id_);

    /**
     * @brief get an existing exchange on the exchange map
     * 
     * @param exchange_id_              unique id of the exchange to look for
     * @return optional<asset_sp_t>     existing exchange, none if not exists
     */
    optional<exchange_sp_t> get_exchange(const string& exchange_id_);

    /// get market price of asset
    double get_market_price(const string& asset_id);

    /// reset exchange map
    void reset_exchange_map()
    {
        for(auto& exchange_pair : this->exchanges)
        {
            exchange_pair.second->reset_exchange();
        }
    }
};  

/// function for creating a shared pointer to a asset
shared_ptr<Exchange> new_exchange(const string &exchange_id);

#endif // ARGUS_EXCHANGE_H
