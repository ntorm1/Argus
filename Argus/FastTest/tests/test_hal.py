import sys
import os
import time
import unittest
import cProfile

import numpy as np
sys.path.append(os.path.abspath('..'))

import FastTest
from Hal import Hal
from FastTest import Portfolio, Exchange, Broker
from FastTest import OrderExecutionType, OrderTargetType

import helpers

class MovingAverageStrategy:
    def __init__(self, hal : Hal) -> None:        
        self.exchange = hal.get_exchange(helpers.test1_exchange_id)
        self.broker = hal.get_broker(helpers.test1_broker_id)
        self.portfolio1 = hal.get_portfolio("master");
                
    def build(self) -> None:
        return
            
    def on_open(self) -> None:
        return
    
    def on_close(self) -> None:
        cross_dict = {}
        
        self.exchange.get_exchange_feature(cross_dict, "FAST_ABOVE_SLOW")

        for asset_id, cross_value in cross_dict.items():
            if cross_value == 1:    
                units = 1
            elif cross_value == 0: 
                units = -1
            self.portfolio1.order_target_size(asset_id, units, "dummy", 
                                    .01,
                                    OrderTargetType.DOLLARS,
                                    OrderExecutionType.EAGER,
                                    -1)

class SimpleStrategy:
    def __init__(self, hal : Hal) -> None:        
        self.exchange = hal.get_exchange(helpers.test1_exchange_id)
        self.broker = hal.get_broker(helpers.test1_broker_id)
        self.portfolio1 = hal.new_portfolio("test_portfolio1",100000.0);
                
    def on_open(self) -> None:
        return
    
    def on_close(self) -> None:
        position = self.portfolio1.get_position(helpers.test2_asset_id)
        close_price = self.exchange.get_asset_feature(helpers.test2_asset_id, "CLOSE")
        if position is None:
            if close_price <= 97.0:
                self.portfolio1.place_market_order(
                    helpers.test2_asset_id,
                    100.0,
                    "dummy",
                    FastTest.OrderExecutionType.EAGER,
                    -1
                )
        elif close_price >= 101.5:
            self.portfolio1.place_market_order(
                    helpers.test2_asset_id,
                    -1 * position.get_units(),
                    "dummy",
                    FastTest.OrderExecutionType.EAGER,
                    -1
                )

    def build(self) -> None:
        return
        
class HalTestMethods(unittest.TestCase):
    def test_hal_run(self):
        hal = helpers.create_simple_hal(logging=0)
        hal.build()
        hal.run()
        assert(True)
        
    
    def test_hal_register_strategy(self):
        hal = helpers.create_simple_hal(logging=0)

        strategy = SimpleStrategy(hal)
        hal.register_strategy(strategy)
        
        hal.build()
        hal.run()
                
        cash_actual = np.array([100000, 100000,  90300, 100450, 100450,  90850.0,])
        nlv_actual = np.array([100000, 100000, 100000, 100450, 100450, 100450.0,])
        
        for portfolio in ["master", "test_portfolio1"]:
            portfolio_history = hal.get_portfolio(portfolio).get_portfolio_history()
            cash_history = portfolio_history.get_cash_history()
            nlv_history = portfolio_history.get_nlv_history()
            
            assert(np.array_equal(cash_actual, cash_history))
            assert(np.array_equal(nlv_actual, nlv_history))

    def test_hal_big(self):
        hal = helpers.create_big_hal(logging = 0, cash = 100000.0)
        
        exchange = hal.get_exchange(helpers.test1_exchange_id)
        
        strategy = MovingAverageStrategy(hal)
        hal.register_strategy(strategy) 
               
        hal.build()

        st = time.time()
        hal.run()
        et = time.time()
        
        execution_time = et - st
        candles = hal.get_candles()
        
        print(f"HAL: candles: {candles:.4f} candles")
        print(f"HAL: execution time: {execution_time:.4f} seconds")
        print(f"HAL: candles per seoncd: {(candles / execution_time):,.3f}")      
        
        #orders = hal.get_order_history()
        #print(orders)
        
        #portfolio_history = hal.get_portfolio("master").get_portfolio_history()
        #nlv_history = portfolio_history.get_nlv_history()
        #print(nlv_history[-1])
        
        assert(True)
     
    
    
if __name__ == '__main__':
    unittest.main()
    #tests % python -m cProfile -s cumtime test_hal.py
    