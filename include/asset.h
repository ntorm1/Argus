//
// Created by Nathan Tormaschy on 4/17/23.
//

#ifndef ARGUS_ASSET_H
#define ARGUS_ASSET_H
#include <cstddef>
#include "pch.h"
#include <cmath>
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

    /// @brief the frequency of the asset rows (1D, 1H, etc..)
    AssetFrequency frequency;

    /// @brief unique id of the asset
    string asset_id;

    /// @brief unique id of the exchange the asset is on
    string exchange_id;

    /// @brief unique id of the broker the asset is listed on 
    string broker_id;

    /// @brief is the asset's datetime index alligend with it's exchange
    bool is_alligned;

    /// @brief index of the open column;
    size_t open_column;

    /// @brief index of the close column
    size_t close_column;

    /// @brief index of the current row the asset is at
    size_t current_index;

    /// @brief pointer to an index asset
    optional<asset_sp_t> index_asset = nullopt;

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

    /// reset asset to start of data
    void reset_asset();

    /// @brief  build the asset
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
    optional<shared_ptr<AssetTracer>> get_tracer(AssetTracerType tracer_type);

    /**
     * @brief build and add a new tracer object to the asset
     * 
     * @param tracer_type type of tracer to add
     * @param lookback    lookback of the tracer
     */
    void add_tracer(AssetTracerType tracer_type, size_t lookback, bool adjust_warmup);

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

    /// test if the function is built
    [[nodiscard]] bool get_is_built() const;

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

    /// get data point from current asset row
    [[nodiscard]] double c_get(size_t column_offset) const;

    /// get data point from asset
    [[nodiscard]] double get(const string &column, size_t row_index) const;

    /// get fixed point rep of current market price
    [[nodiscard]] double get_market_price(bool on_close) const;

    /// get the current datetime of the asset
    [[nodiscard]] long long *get_asset_time() const;

    /// get read only numpy array of the asset's datetime index
    py::array_t<long long> get_datetime_index_view();

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

    /// step the asset forward in time
    void step();

private:
    /// @brief has the asset been built
    bool is_built = false;

    /// @brief does the asset own the underlying data pointer
    bool is_view = false;

    /// @brief map between column name and column index
    std::unordered_map<string, size_t> headers;

    /// @brief index of the asset (ns epoch time stamp)
    long long *datetime_index;

    /// @brief underlying data of the asset
    double * data;

    /// @brief pointer to the current row
    double * row;

    /// @brief number of rows in the asset data
    size_t rows;

    /// @brief number of columns in the asset data
    size_t cols;

    /// @brief warmup period, i.e. number of rows to skip
    size_t warmup = 0;

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
    AssetTracer(Asset* parent_asset_, size_t lookback_) : 
        parent_asset(parent_asset_), lookback(lookback_){};

    /// Asset Tracer default destructor 
    virtual ~AssetTracer() = default;

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

    /// @brief size of the lookback window of tracer
    size_t lookback;
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
    void build() override {};

    // pure virtual function to reset the tracer
    void reset() override {};

    double volatility = 0;
    double sum_sqaures = 0.0;
    double sum = 0.0 ;
    double mean = 0;

private:
    /// @brief array window into the asset's close column
    Argus::ArrayWindow<double> asset_window;
};

class BetaTracer : AssetTracer
{
public:
    BetaTracer(Asset* parent_asset_, size_t lookback_);

    //Type of the tracer
    AssetTracerType tracer_type() const override {return AssetTracerType::Beta;}

    /// pure virtual function called on parent asset step
    void step() override {}

    // pure virtual function to build the tracer
    void build() override {};

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

};

#endif // ARGUS_ASSET_H
