#include "pch.h"

#include <cmath>
#include "asset.h"
#include "containers.h"
#include "settings.h"
#include "utils_array.h"
#include "utils_string.h"
#include "fmt/core.h"
#include "pybind11/numpy.h"
#include <pybind11/stl.h>


namespace py = pybind11;

using namespace std;
using namespace Argus;

using asset_sp_t = Asset::asset_sp_t;

Asset::Asset(string asset_id_, string exchange_id_, string broker_id_, size_t warmup_,  AssetFrequency frequency_)              
{
    this->asset_id = std::move(asset_id_);
    this->exchange_id = std::move(exchange_id_);
    this->broker_id = std::move(broker_id_);

    this->rows = 0,
    this->cols = 0,
    this->current_index = warmup_;

    this->warmup = warmup_;
    this->frequency = frequency_;
    this->is_built = false;
}

Asset::~Asset()
{

#ifdef DEBUGGING
    printf("MEMORY:   CALLING ASSET %s DESTRUCTOR ON: %p \n", this->asset_id.c_str(), this);
#endif

    if (!this->is_built)
    {
        return;
    }
    if(this->is_view)
    {
        return;
    }

    // delete the underlying data
    delete[] this->data;

    // delete the datetime index
    delete[] this->datetime_index;

#ifdef DEBUGGING
    printf("MEMORY:   DESTRUCTOR ON: %p COMPLETE \n", this);
#endif
}

void Asset::reset_asset()
{   
    // move datetime index and data pointer back to start
    this->current_index = this->warmup;
    this->row = &this->data[this->warmup*this->cols];
}

void Asset::build()
{
    for(auto& tracer : this->tracers)
    {   
        tracer->build();
    }
}

void Asset::set_warmup(size_t warmup_)
{
    if(!this->is_built)
    {
        ARGUS_RUNTIME_ERROR(ArgusErrorCode::NotBuilt);
    }
    // move the asset forward forward #warmup rows 
    this->warmup = warmup_;
    this->current_index = warmup_;
    this->row = &this->data[warmup * this->cols];
}

string Asset::get_asset_id() const
{
    return this->asset_id;
}

bool Asset::get_is_built() const
{
    return this->is_built;
}

void Asset::load_headers(const vector<std::string> &columns)
{
    auto column_indecies = parse_headers(columns);
    this->open_column = std::get<0>(column_indecies);
    this->close_column = std::get<1>(column_indecies);

    size_t i = 0;
    for (const auto &column_name : columns)
    {
        this->headers.emplace(column_name, i);
        i++;
    }
}

asset_sp_t Asset::fork_view()
{
    // asset must be built in order to be forked
    if(!this->is_built)
    {
        ARGUS_RUNTIME_ERROR(ArgusErrorCode::NotBuilt);
    }

    auto asset_view = std::make_shared<Asset>(
        this->asset_id, 
        this->exchange_id, 
        this->broker_id, 
        this->warmup
    );
    // set is_view true (don't deallocate memory on destruction)
    asset_view->is_view = true;
    asset_view->is_built = true;
    asset_view->is_alligned = this->is_alligned;

    asset_view->headers = this->headers;
    asset_view->load_view(
        this->data, 
        this->datetime_index,
        this->rows, 
        this->cols
    );
    asset_view->open_column = this->open_column;
    asset_view->close_column = this->close_column;
    asset_view->current_index = this->current_index;
    asset_view->row = this->row;
    return asset_view;
}

void Asset::load_view(double *data_, long long *datetime_index_, size_t rows_, size_t cols_){
#ifdef DEBUGGING
    printf("MEMORY: CALLING ASSET %s load_data() ON: %p \n", this->asset_id.c_str(), this);
#endif  

    // set data to point to the existing allocated data
    this->data = data_;

    // set the datetimes index to point to existing datetime index
    this->datetime_index = datetime_index_;

    // set the asset matrix size
    this->rows = rows_;
    this->cols = cols_;

    //is built and is a view
    this->is_view = true;
    this->is_built = true;

    this->row = &this->data[0];
#ifdef DEBUGGING
    printf("MEMORY:   asset %s datetime index at: %p \n", this->asset_id.c_str(), this->datetime_index);
    printf("MEMORY:   asset %s load_data() allocated at: %p  \n", this->asset_id.c_str(), this);
#endif
}

void Asset::load_data(const double *data_, const long long *datetime_index_, size_t rows_, size_t cols_)
{
#ifdef DEBUGGING
    printf("MEMORY: CALLING ASSET %s load_data() ON: %p \n", this->asset_id.c_str(), this);
#endif
    if (this->is_built)
    {
        throw runtime_error("asset is already built");
    }

    // allocate data array
    this->data = new double[rows_ * cols_];

    // allocate datetime index
    this->datetime_index = new long long[rows_];

    // set the asset matrix size
    this->rows = rows_;
    this->cols = cols_;

    // copy the data from a column formated 1d array ([col1_0, col1_1, col2_0, col2_1])
    for (int j = 0; j < cols_; j++) {
        auto input_col_start = j * rows_;
        for (int i = 0; i < rows_; i++) {
            auto value = data_[input_col_start + i];
            data[i * cols_ + j] = value;
        }
    }

    // copy the datetime index into the asset
    for (int i = 0; i < rows_; i++)
    {
        this->datetime_index[i] = datetime_index_[i];
    }

    //set row pointer to first row 
    this->row = &this->data[this->warmup * this->cols];

    // set build flag to true after copying data
    this->is_built = true;

#ifdef DEBUGGING
    printf("MEMORY:   asset %s datetime index at: %p \n", this->asset_id.c_str(), this->datetime_index);
    printf("MEMORY:   asset %s load_data() allocated at: %p  \n", this->asset_id.c_str(), this);
#endif
}

void Asset::py_load_data(
    const py::buffer &py_data,
    const py::buffer &py_datetime_index,
    size_t rows_,
    size_t cols_,
    bool is_view)
{
    if(headers.size() == 0)
    {   
        ARGUS_RUNTIME_ERROR(ArgusErrorCode::InvalidArrayLength);
    }

    py::buffer_info data_info = py_data.request();
    py::buffer_info datetime_index_info = py_datetime_index.request();

    // cast the python buffers to raw pointer
    auto data_ = static_cast<double *>(data_info.ptr);
    auto datetime_index_ = static_cast<long long *>(datetime_index_info.ptr);

    // pass raw pointer to c loading function and copy data
    if(!is_view)
    {
        this->load_data(data_, datetime_index_, rows_, cols_);
    }
    // pass raw pointer and mirror the asset pointer to the data passed in the py buffers
    else
    {
        this->load_view(data_, datetime_index_, rows_, cols_);
    }
}

double Asset::c_get(size_t column_index) const
{
    // derefence data pointer at current row plus column offset
    return *(this->row - this->cols + column_index);
}

double Asset::get(const std::string &column, size_t row_index) const
{
    // fetch the column index of it exists
    size_t column_index;
    try
    {
        column_index = this->headers.at(column);
    }
    catch (const std::out_of_range &e)
    {
        // Catch the exception and re-raise it as a Python KeyError
        throw py::key_error(e.what());
    }
    // make sure the row index is valid
    if (row_index >= this->rows)
    {
        ARGUS_RUNTIME_ERROR(ArgusErrorCode::IndexOutOfBounds);
    }
    return this->data[row_index * this->cols + column_index];
}

double Asset::get_market_price(bool on_close) const
{

    #ifdef ARGUS_RUNTIME_ASSERT
    //make sure row pointer is not out of bounds
    ptrdiff_t index = this->row - this->data; 
    auto size = this->rows * this->cols;
    assert(index - this->cols  < size);
    #endif

    //subtract this->cols to move back row, then get_market_view is called, asset->step()
    //is called so we need to move back a row when accessing asset data
    if (on_close)
        return *(this->row - this->cols + this->close_column);
    else
        return *(this->row - this->cols + this->open_column);
}

double Asset::get_asset_feature(const string& column_name, int index)
{

    #ifdef ARGUS_RUNTIME_ASSERT
    //make sure row pointer is not out of bounds
    ptrdiff_t ptr_index = this->row - this->data; 
    auto size = this->rows * this->cols;
    assert(ptr_index - this->cols < size);
    assert(index <= 0);
    #endif

    //subtract this->cols to move back row, then get_market_view is called, asset->step()
    //is called so we need to move back a row when accessing asset data
    auto column_offset = this->headers.find(column_name);
    auto row_offset = static_cast<int>(this->cols) * index;

    #ifdef ARGUS_RUNTIME_ASSERT
    if(column_offset == this->headers.end()){
        ARGUS_RUNTIME_ERROR(ArgusErrorCode::InvalidDataRequest);
    }
    #endif

    //prevent acces index < 0
    assert(row_offset + ptr_index > 0);
    return *(this->row - this->cols + column_offset->second + row_offset);
}

py::array_t<double> Asset::get_column(const string& column_name, size_t length)
{
    if(length >= this->current_index)
    {
        ARGUS_RUNTIME_ERROR(ArgusErrorCode::IndexOutOfBounds);
    }

    auto column_offset = this->headers.find(column_name);
    auto row_offset = static_cast<int>(this->cols) * length;
    
    auto column_start = this->row - this->cols + column_offset->second - row_offset;
    return py::array( 
        py::buffer_info
            (
                column_start,                               /* Pointer to buffer */
                sizeof(double),                          /* Size of one scalar */
                py::format_descriptor<double>::format(), /* Python struct-style format descriptor */
                1,                                      /* Number of dimensions */
                { length },                 /* Buffer dimensions */
                { sizeof(double) * this->cols}
            )
    );
}

double* Asset::get_column_ptr(size_t column_index)
{
    return this->data + column_index;
}

long long *Asset::get_datetime_index(bool warmup_start) const
{   
    if(warmup_start)
    {
        return &this->datetime_index[warmup];
    }
    else
    {
        return &this->datetime_index[0];
    }
}

py::array_t<long long> Asset::get_datetime_index_view()
{
    if (!this->is_built)
    {
        ARGUS_RUNTIME_ERROR(ArgusErrorCode::NotBuilt);

    }
    if (this->rows == 0)
    {
        ARGUS_RUNTIME_ERROR(ArgusErrorCode::InvalidArrayLength);

    }
    return to_py_array(
        this->datetime_index,
        this->rows,
        true);
};

long long *Asset::get_asset_time() const
{
    if (this->current_index == this->rows)
    {
        return nullptr;
    }
    else
    {
        return &this->datetime_index[this->current_index];
    }
}

void Asset::goto_datetime(long long datetime)
{
    //goto date is beyond the datetime index
   if(datetime >= this->datetime_index[this->rows-1])
    {
        this->current_index = this->rows;
    }
    
    // search for datetime in the index
    for(int i = this->current_index; i < this->rows; i++)
    {   
        //is >= right?
        if(this->datetime_index[i] >= datetime)
        {
            this->row += (this->cols * (i-this->warmup));
            this->current_index = i;
            return;
        }
    }

    throw std::runtime_error("failed to find datetime in asset goto");    
}

std::shared_ptr<Asset> new_asset(
    const string &asset_id,
    const string& exchange_id,
    const string& broker_id,
    size_t warmup)
{
    return std::make_shared<Asset>(asset_id, exchange_id, broker_id,warmup);
}

void Asset::step(){
    // move the row pointer forward to the next row
    this->row += this->cols;

    // move the current index forward
    this->current_index++; 

    // move any tracers forward
    if(this->tracers.size())
    {
        for(auto& tracer : this->tracers)
        {
            tracer->step();
        }
    }
}

optional<shared_ptr<AssetTracer>> Asset::get_tracer(AssetTracerType tracer_type){
    auto tracer = vector_get(
        this->tracers,
        [tracer_type](auto tracer) { return tracer->tracer_type() == tracer_type; }
    );
    return tracer;
};

void Asset::add_tracer(AssetTracerType tracer_type, size_t lookback, bool adjust_warmup)
{
    switch (tracer_type)
    {
    case AssetTracerType::Volatility:
        this->tracers.push_back( std::make_shared<VolatilityTracer>(this, lookback, adjust_warmup));
    default:
        ARGUS_RUNTIME_ERROR(ArgusErrorCode::NotImplemented);
    }
}

ArrayWindow<double> init_array_window(Asset* asset, size_t lookback)
{
    double* start_ptr;
    size_t start_index;
    // if the asset's current index is greater than the lookback we have all the data we need
    // so set the start pointer to the current row minus lookback rows.
    if(asset->current_index >= lookback)
    {   
        start_ptr = asset->get_row() - (lookback * asset->get_cols());
        start_index = asset->current_index - lookback;
    }
    else
    {
        start_ptr = asset->get_row() + asset->close_column;
        start_index = 0;
    }

    auto array_window = ArrayWindow<double>(
        start_ptr,
        asset->get_cols(),
        lookback
    );
    // set the start pointer index based on what row window start pointer is pointing to
    array_window.start_ptr_index = start_index;

    // if the start index is not 0 then the window is fully populated
    if(!start_index)
    {
        array_window.rows_needed = 0;
    }
    else
    {
        array_window.rows_needed = lookback - asset->current_index;
    }
    return array_window;
}

BetaTracer::BetaTracer(Asset* parent_asset_, size_t lookback_) 
        : AssetTracer(parent_asset_, lookback_)
{
    // asset must have a index linked to it 
    if(!parent_asset_->index_asset.has_value())
    {
        ARGUS_RUNTIME_ERROR(ArgusErrorCode::InvalidTracerAsset);
    }
    else
    {
        this->index_asset = parent_asset_->index_asset.value().get();
    }

    // the parent asset and index asset must have the same frequencies.
    if(this->parent_asset->frequency != this->index_asset->frequency)
    {
        ARGUS_RUNTIME_ERROR(ArgusErrorCode::InvalidAssetFrequency);
    }

    // assets must be build for adding tracer
    if(!parent_asset->get_is_built() || !index_asset->get_is_built())
    {
        ARGUS_RUNTIME_ERROR(ArgusErrorCode::NotBuilt);
    }

    // make sure the lookback period is not greater than the number of rows loaded
    if(parent_asset->get_rows() < lookback || index_asset->get_rows() < lookback)
    {   
        ARGUS_RUNTIME_ERROR(ArgusErrorCode::IndexOutOfBounds);
    }
    
    this->asset_window =  init_array_window(parent_asset_, lookback);

    // build the parent asset window
    auto t0 = this->parent_asset->get_datetime_index()[this->asset_window.start_ptr_index];

    // take the datetime of the starting row of the parent asset and search for it in the 
    // index asset's datetime index. Know it has value because it is contained
    auto index_start = array_find(
        this->index_asset->get_datetime_index(),
        this->index_asset->get_rows(),
        t0
    );
    assert(index_start.has_value());

    // get pointer to the index starting position
    double* index_start_ptr = this->index_asset->get_data() + (index_start.value()*index_asset->get_cols());

    // build the window into the index asset
    this->index_window = ArrayWindow<double>(
        index_start_ptr,
        index_asset->get_cols(),
        this->lookback
    );

    //TODO
    auto index_vol_tracer = this->index_asset->get_tracer(AssetTracerType::Volatility);
    if(!index_vol_tracer.has_value())
    {
        //shared_ptr<VolatilityTracer> tracer = std::make_shared<VolatilityTracer>(this->parent_asset, this->lookback); 
        //this->index_volatility = &index_vol_tracer.value()->volatility;
    }
    else
    {
        
    }
}

VolatilityTracer::VolatilityTracer(Asset* parent_asset_, size_t lookback_, bool adjust_warmup) 
        : AssetTracer(parent_asset_, lookback_)
{
    // make sure the lookback period is not greater than the number of rows loaded
    if(parent_asset->get_rows() < lookback)
    {   
        ARGUS_RUNTIME_ERROR(ArgusErrorCode::IndexOutOfBounds);
    }
    // if the asset hasn't been built yet, allow for lookback to adjust the warmup needed for the asset
    if(!parent_asset->get_is_built() && adjust_warmup)
    {
        if(lookback_ > parent_asset_->get_warmup())
        {
            parent_asset_->set_warmup(lookback_);
        }   
    }

    // build the asset window
    this->asset_window = init_array_window(parent_asset_, lookback);

    // volatility calculations given the passed data
    double previous;
    int i = 0;
    for(auto it = this->asset_window.begin(); it != this->asset_window.end(); it++)
    {
        if(i = 0)
        {
            previous = *it;
        }
        else
        {
            auto pct_change = (*it - previous) / previous;
            this->sum += pct_change;
            this->sum_sqaures += pow(pct_change, 2);
            previous = *it;
        }
        i++;
    }

    this->mean = this->sum / i;

    // if the window is fully loaded set the volatility
    if(parent_asset_->current_index >= lookback)
    {
        this->volatility = (this->sum_sqaures / this->lookback) - pow(this->mean, 2);
    }
}

void VolatilityTracer::step()
{
    // calculate pct change
    double previous_value = *(this->asset_window.end_ptr - this->asset_window.stride);
    double new_value = (*this->asset_window.end_ptr - previous_value) / previous_value;

    // get the last value about to be popped off 
    double old_value = *this->asset_window.start_ptr;

    //move ptrs forward
    this->asset_window.step();
    this->sum += new_value;
    this->sum_sqaures += pow(new_value, 2);

    // asset not fully loaded
    if(!this->asset_window.rows_needed)
    {
        this->sum -= old_value;
        this->sum_sqaures -= pow(old_value,2);
        this->mean = this->sum / this->lookback;
        this->volatility = (this->sum_sqaures / this->lookback) -  pow(this->mean,2);
    }
    // still accumulating values
    else
    {        
        this->asset_window.rows_needed--;
    }
}