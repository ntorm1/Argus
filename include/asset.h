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

#include "utils_array.h"
#include "containers.h"

namespace py = pybind11;
using namespace std;

class Asset;
class AssetTracer;

enum AssetTracerType
{
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

    AssetFrequency frequency;

    /// unique id of the asset
    string asset_id;

    /// unique id of the exchange the asset is on
    string exchange_id;

    /// unique id of the broker the asset is listed on 
    string broker_id;

    /// is the asset's datetime index alligend with it's exchange
    bool is_alligned;

    /// warmup period, i.e. number of rows to skip
    size_t warmup = 0;

    /// index of the open column;
    size_t open_column;

    /// index of the close column
    size_t close_column;

    /// index of the current row the asset is at
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

    /// move the asset to an exact point in time
    void goto_datetime(long long datetime);
    
    /// is the the last row in the asset
    bool is_last_view() {return this->current_index == this->rows;};

    /// return the memory address of the underlying asset opbject
    auto get_mem_address(){return reinterpret_cast<std::uintptr_t>(this); }

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

    /// load in the headers of an asset from python list
    void load_headers(const vector<string> &headers);

    /// load the asset data in from a pointer, copy to dynamically allocated double**
    void load_data(const double *data, const long long *datetime_index, size_t rows, size_t cols);
    void load_view(double *data, long long *datetime_index, size_t rows, size_t cols);

    /// load the asset data using a python buffer
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

    /// step the asset forward in time
    void step();

private:
    /// has the asset been built
    bool is_built = false;

    /// does the asset own the underlying data pointer
    bool is_view = false;

    /// map between column name and column index
    std::unordered_map<string, size_t> headers;

    /// datetime index of the asset (ns epoch time stamp)
    long long *datetime_index;

    /// underlying data of the asset
    double * data;

    /// @brief pointer to the current row
    double * row;

    /// number of rows in the asset data
    size_t rows;

    /// number of columns in the asset data
    size_t cols;

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

    /// pure virtual function called on parent asset step
    virtual void step() = 0;

    // pure virtual function to build the tracer
    virtual void build() = 0;

    // pure virtual function to reset the tracer
    virtual void reset() = 0;

    // is the tracer ready to be accessed
    bool is_built(){this->parent_asset->current_index >= this->lookback;};

protected:
    /// pointer to the parent asset of the tracer
    Asset* parent_asset;

    /// lookback period of the tracer
    size_t lookback;
};

class BetaTracer : AssetTracer
{
public:
    BetaTracer(Asset* parent_asset_, size_t lookback_);

    /// pure virtual function called on parent asset step
    void step() override {}

    // pure virtual function to build the tracer
    void build() override;

    // pure virtual function to reset the tracer
    void reset() override {};

private:
    /// pointer to the index asset
    Asset* index_asset;

    /// parent asset window
    Argus::ArrayWindow<double> asset_window;

    /// index asset window
    Argus::ArrayWindow<double> index_window;

};

#endif // ARGUS_ASSET_H
