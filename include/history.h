//
// Created by Nathan Tormaschy on 4/22/23.
//

#ifndef ARGUS_HISTORY_H
#define ARGUS_HISTORY_H
#include <vector>
#include <memory>

#include "settings.h"
#include "trade.h"
#include "order.h"
#include "position.h"

using namespace std;

class History{
private:
    /// history of all orders
    vector<shared_ptr<Order>> orders;

    /// history of all trades
    vector<shared_ptr<Trade>> trades;

    /// history of all positions
    vector<shared_ptr<Position>> positions;

public:

    void remember_order(shared_ptr<Order> order){
#ifdef ARGUS_RUNTIME_ASSERT
        assert(order.use_count() == 1);
#endif
        this->orders.push_back(std::move(order));
    };

    void remember_trade(shared_ptr<Trade> trade){        
#ifdef ARGUS_RUNTIME_ASSERT
        //printf("1\n");
        assert(trade);
        //printf("2\n");
        //assert no one hold sp to trade
        assert(trade.use_count() == 1); 
        //printf("3\n");  
        
         //assert trade id was given
        assert(trade->get_trade_id() >= 0);
        //printf("4\n");
#endif
        //printf("trade avg price: %s \n", trade->get_asset_id().c_str());
        //printf("trade avg price: %ld \n", trade->get_mem_address());
        //this->trades.emplace_back(trade);
        //printf("5\n");
    };

    void remember_position(shared_ptr<Position> position){
        printf("a\n");
#ifdef ARGUS_RUNTIME_ASSERT
        assert(position.use_count() == 1);
#endif
        printf("b\n");
        printf("%f\n", position->get_units());
        this->positions.push_back(std::move(position));
        printf("c\n");
    };


};

#endif //ARGUS_HISTORY_H
