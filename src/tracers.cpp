#include "pch.h"
#include <stdexcept>

#include "asset.h"
#include "portfolio.h"

void Portfolio::add_tracer(PortfolioTracerType tracer_type)
{
    this->portfolio_history->add_tracer(tracer_type);
}

shared_ptr<PortfolioTracer> Portfolio::get_tracer(PortfolioTracerType tracer_type)
{
    auto tracer =  this->portfolio_history->get_tracer(tracer_type);

    if(!tracer)
    {
        throw std::runtime_error("tracer does not exist");
    }
    return tracer;
}

void PortfolioHistory::add_tracer(PortfolioTracerType tracer_type)
{
    // test to see if tracer exists

    auto tracer = this->get_tracer(tracer_type);
    if(tracer)
    {
        throw std::runtime_error("tracer already exists");
    }

    switch (tracer_type) 
    {
        case PortfolioTracerType::Value:
        {
            this->tracers.push_back(std::make_shared<ValueTracer>(this->parent_portfolio));
            break;
        }
        case PortfolioTracerType::Event:
        {
            auto event_tracer = std::make_shared<EventTracer>(this->parent_portfolio);
            this->parent_portfolio->set_event_tracer(event_tracer);
            this->tracers.push_back(event_tracer);
            break;
        }
        case PortfolioTracerType::PortfolioBeta:
        {
            auto tracer = std::make_shared<PortfolioBetaTracer>(this->parent_portfolio);
            this->parent_portfolio->set_beta(&tracer->beta);
            this->tracers.push_back(tracer);
            break;
        }
    }
}

shared_ptr<PortfolioTracer> PortfolioHistory::get_tracer(PortfolioTracerType tracer_type){
    auto tracer = vector_get(
        this->tracers,
        [tracer_type](auto tracer) { return tracer->tracer_type() == tracer_type; }
    );
    if(tracer.has_value())
    {
        return tracer.value();
    }
    else
    {
        return nullptr;
    }
};

void PortfolioHistory::build(size_t portfolio_eval_length)
{
    for(auto& tracer : this->tracers)
    {
        tracer->build(portfolio_eval_length);
    }
}

void PortfolioHistory::reset(bool clear_history)
{
    for(auto& tracer : this->tracers)
    {
        if(tracer->tracer_type() == PortfolioTracerType::Event)
        {
            if(clear_history)
            {
                tracer->reset();
            }
        }
        else
        {
            tracer->reset();
        }
    }
}

void PortfolioHistory::update(long long datetime){
    for(auto& tracer : this->tracers)
    {
        tracer->step(datetime);
    }
}