//
// Created by Nathan Tormaschy on 4/17/23.
//

#ifndef ARGUS_ASSET_H
#define ARGUS_ASSET_H
#include <cstddef>
#include "pch.h"
#include <cmath>
#include <optional>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>

#include "containers.h"

namespace py = pybind11;
using namespace std;

class Asset;
class AssetTracer;

enum AssetTracerType
{
    Volatility,
    Beta
};

 enum AssetFrequency
 {
    Daily
 };


class Asset
{ 
public:
    typedef shared_ptr<Asset> asset_sp_t;

    /**
     * @brief Asset object constructor
     * 
     * @param asset_id      unique id of the asset
     * @param exchange_id   unique id of the exchange the asset is registerd on
     * @param broker_id     unique id of the broker the asset is registered on 
     * @param warmup        warmup period of the asset (number of rows skipped but still loaded)
     * @param frequency_    frequency of the rows (1D, 1H, etc...)
     */
    Asset(
        string asset_id, 
        string exchange_id, 
        string broker_id, 
        size_t warmup = 0, 
        AssetFrequency frequency_ = AssetFrequency::Daily
    );

    /// asset destructor
    ~Asset();

    AssetFrequency frequency;   ///< the frequency of the asset rows (1D, 1H, etc..)
       
    bool is_alligned;           ///< is the asset's datetime index alligend with it's exchange

    string asset_id;            ///< unique id of the asset
    string exchange_id;         ///< unique id of the exchange the asset is on
    string broker_id;           ///< unique id of the broker the asset is listed on 

    size_t open_column;         ///< index of the open column;
    size_t close_column;        ///< index of the close column
    size_t current_index;       ///< index of the current row the asset is at

    optional<asset_sp_t> index_asset = nullopt; /// < pointer to an index asset

    /// @brief vector of tracers registered to the asset
    vector<shared_ptr<AssetTracer>> tracers;

    /**
     * @brief fork an asset into a view, the new object will be a new object entirly except for the 
     *        data and datetime index pointers, they will point to this existing object (i.e. no dyn alloc)
     * 
     * @return asset_sp_t new asset object in a smart pointer
     */
    asset_sp_t fork_view();

    /**
     * @brief register a new index asset
     * 
     * @param index_asset_ smart pointer to linked index asset
     */
    void register_index_asset(asset_sp_t index_asset_){this->index_asset = index_asset_;};

    /**
     * @brief reset the asset to the begining of it's data
     * 
     */
    void reset_asset();

    /**
     * @brief build an asset and it's corresponding tracers
     * 
     */
    void build();

    /**
     * @brief move the asset to an exact point in time
     * 
     * @param datetime ns epoch time to move the asset to
     */
    void goto_datetime(long long datetime);
    
    /**
     * @brief is the asset done streaming, occurs when the exchange get_market_view steps the asset forward
     * to the last step
     * 
     * @return true asset is done streaming
     * @return false asset is still streaming
     */
    bool is_last_view() {return this->current_index == this->rows;};

    /**
     * @brief returns the memory address of the asset (used for testing)
     * 
     * @return uintptr pointer to the memory address of this asset
     */
    auto get_mem_address(){return reinterpret_cast<std::uintptr_t>(this); }

    /**
     * @brief find an existing tracer registered to the asset
     * 
     * @param tracer_type type of tracer to search for
     * @return optional<shared_ptr<AssetTracer>> nullopt if not exists, else return shared pointer to the tracer
     */
    optional<shared_ptr<AssetTracer>> get_tracer(AssetTracerType tracer_type) const;

    /**
     * @brief Get the value set be a specific tracer by type
     * 
     * @param tracer_type the type of tracer to get (not searching for, just deref appropriate pointer)
     * @return current value of the tracer
     */
    double get_tracer_value(AssetTracerType tracer_type) const;

    /**
     * @brief build and add a new tracer object to the asset
     * 
     * @param tracer_type type of tracer to add
     * @param lookback    lookback of the tracer
     */
    void add_tracer(AssetTracerType tracer_type, size_t lookback, bool adjust_warmup = false);

    /// @brief get a pointer to the current row of the asset
    /// @return const pointer to the underlying row data
    double * get_row() const {return this->row;}

    /// @brief get the number of rows of data remaining for the asset
    /// @return number of rows remaining, including the current one
    size_t get_rows_remaining() const{return this->rows - this->current_index - 1;}
    
    /// return the number of rows in the asset
    [[nodiscard]] size_t get_rows() const { return this->rows; }

    /// return the number of columns in the asset
    [[nodiscard]] size_t get_cols() const { return this->cols; }

    /// return the id of an asset
    [[nodiscard]] string get_asset_id() const;

    /// return pointer to the first element of the datetime index;
    [[nodiscard]] long long *get_datetime_index(bool warmup_start = false) const;

    /**
     * @brief Get pointer to the asset's underlying data
     * 
     * @return double* pointer to first element of the asset's data
     */
    double* get_data() {return this->data;};

    [[nodiscard]] bool get_is_built() const {return this->is_built;};

    /**
     * @brief load in the headers from a vector of strings and assign them size_t indexs
     * 
     * @param headers a reference of a vector of strings representing the column names of the data
     */
    void load_headers(const vector<string> &headers);

    /**
     * @brief load in the data from a a column major array of values and datetime index
     * 
     * @param data              pointer to the start of the data
     * @param datetime_index    pointer to the start of the datetime index
     * @param rows              number of rows in the data
     * @param cols              number of columns in the data
     */
    void load_data(const double *data, const long long *datetime_index, size_t rows, size_t cols);
    
    // NOTE: only for test use
    void load_view(double *data, long long *datetime_index, size_t rows, size_t cols);

    /**
     * @brief load in data from python using numpy array interface
     * 
     * @param data              a py buffer object holding a column major numpy array of the asset's values
     * @param datetime_index    a py buffer object olding the datetime index of the asset
     * @param rows              number of rows in the asset
     * @param cols              number of columns in the asset
     * @param is_view           is_view leads to 0 copy asset creation (test use only, very bad performance)
     */
    void py_load_data(
        const py::buffer &data, 
        const py::buffer &datetime_index, 
        size_t rows, 
        size_t cols,
        bool is_view);

    /// @brief get data point from current asset row
    [[nodiscard]] double c_get(size_t column_offset) const;

    /// @brief get data point from asset
    [[nodiscard]] double get(const string &column, size_t row_index) const;

    /// @brief get fixed point rep of current market price
    [[nodiscard]] double get_market_price(bool on_close) const;

    /// @brief get the current datetime of the asset
    [[nodiscard]] long long *get_asset_time() const;

    /// @brief get read only numpy array of the asset's datetime index
    py::array_t<long long> get_datetime_index_view();
    
    double get_volatility() const; ///< get the volatility of the asset 
    double get_beta()       const; ///< get the beta of the assset

    size_t get_warmup(){return this->warmup;}

    /**
     * @brief Get specific data point from asset object
     * 
     * @param column_name name of the column to look at
     * @param index row index to look at, 0 is current, -1 is previous, ...
     * @return double value at that location
     */
    [[nodiscard]] double get_asset_feature(const string& column_name, int index = 0);

    /**
     * @brief Get a column from the asset, end index is the current value
     * 
     * @param column_name name of the column to retrieve
     * @param length lookback period, i.e. 10 will get last 10 values including current
     * @return py::array_t<double> column values
     */
    [[nodiscard]] py::array_t<double> get_column(const string& column_name, size_t length);

    /**
     * @brief Get a pointer to the start of a particular column
     * 
     * @param column_index index of the column to get
     * @return double* pointer to the start of the column
     */
    double* get_column_ptr(size_t column_index);

    /**
     * @brief Set the warmup of the asset object, moves the row pointer to index so data must be loaded already
     * 
     * @param warmup number of rows to skip in warmup period
     */
    void set_warmup(size_t warmup);

    /**
     * @brief Set the volatility to point to an asset tracers value
     * 
     * @param volatility_ pointer to the volatility set by the tracer
     */
    void set_volatility(double* volatility_){this->volatility = volatility_;}

    /**
     * @brief Set the beta to point to an asset tracers value
     * 
     * @param beta_ pointer to the beta set by the tracer
     */
    void set_beta(double* beta_){this->beta = beta_;}

    /// step the asset forward in time
    void step();

private:
    bool is_built = false;      ///< has the asset been built
    bool is_view =  false;       ///< does the asset own the underlying data pointer

    /// @brief map between column name and column index
    std::unordered_map<string, size_t> headers;

    long long*  datetime_index; ///< datetime index of the asset (ns epoch time stamp)
    double*     data;             ///< underlying data of the asset
    double*     row;              ///< pointer to the current row

    size_t rows;       ///< number of rows in the asset data
    size_t cols;       ///< number of columns in the asset data
    size_t warmup = 0; ///< warmup period, i.e. number of rows to skip

    optional<double*> volatility = nullopt; ///< optional pointer to a voltaility tracer's value
    optional<double*> beta       = nullopt; ///< optional pointer to a beta tracer's value
};

/// function for creating a shared pointer to a asset
std::shared_ptr<Asset> new_asset(
    const string &asset_id, 
    const string& exchange_id, 
    const string& broker_id,
    size_t warmup = 0    
);

/// function for identifying index locations of open and close column
tuple<size_t, size_t> parse_headers(const vector<std::string> &columns);

class AssetTracer
{
public:
    /// Asset Tracer constructor 
    AssetTracer(Asset* parent_asset_, size_t lookback_);

    /// Asset Tracer default destructor 
    virtual ~AssetTracer() = default;

    /// @brief size of the lookback window of tracer
    size_t lookback;

    /**
     * @brief Get the type of the tracer
     * 
     * @return PortfolioTracerType 
     */
    virtual AssetTracerType tracer_type() const = 0;

    /// pure virtual function called on parent asset step
    virtual void step() = 0;

    // pure virtual function to build the tracer
    virtual void build() = 0;

    // pure virtual function to reset the tracer
    virtual void reset() = 0;

    // is the tracer ready to be accessed
    bool is_built(){return this->parent_asset->current_index >= this->lookback;};

protected:
    /// @brief pointer to the parent asset of the tracer
    Asset* parent_asset;

};

class VolatilityTracer : public AssetTracer
{
public:
    VolatilityTracer(Asset* parent_asset_, size_t lookback_, bool adjust_warmup = false);

    //Type of the tracer
    AssetTracerType tracer_type() const override {return AssetTracerType::Volatility;}

    /// pure virtual function called on parent asset step
    void step() override;

    // pure virtual function to build the tracer
    void build() override;

    // pure virtual function to reset the tracer
    void reset() override {};

    double* get_volatility(){return &this->volatility;}

    double volatility = 0;
    double sum_sqaures = 0.0;
    double sum = 0.0 ;

private:
    /// @brief array window into the asset's close column
    Argus::ArrayWindow<double> asset_window;
};

class BetaTracer : AssetTracer
{
public:
    BetaTracer(Asset* parent_asset_, size_t lookback_, bool adjust_warmup = false);

    //Type of the tracer
    AssetTracerType tracer_type() const override {return AssetTracerType::Beta;}

    /// pure virtual function called on parent asset step
    void step() override {}

    // pure virtual function to build the tracer
    void build() override;

    // pure virtual function to reset the tracer
    void reset() override {};

private:
    /// pointer to the index asset
    Asset* index_asset;

    /// pointer to the index volatility
    double* index_volatility;

    /// parent asset window
    Argus::ArrayWindow<double> asset_window;

    /// index asset window
    Argus::ArrayWindow<double> index_window;

    double sum_products = 0.0;         ///< Running sum of products
    double sum_parent = 0.0;           ///< Running sum of observations for variable parent asset
    double sum_index = 0.0;            ///< Running sum of observations for variable index asset
    double sum_parent_squared = 0.0;   ///< Running sum of squared observations for variable parent asset
    double sum_index_squared = 0.0;    ///< Running sum of squared observations for variable index asset
    double beta = 0.0;                 ///< beta of the parent asset
};

#endif // ARGUS_ASSET_H
