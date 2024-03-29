import sys
import os
import time
import unittest
import cProfile

import numpy as np
os.add_dll_directory("C:\\msys64\\mingw64\\bin")
sys.path.append(os.path.abspath('..'))
sys.path.append(os.path.abspath('../lib'))

import FastTest
from Hal import Hal
from FastTest import Portfolio, Exchange, Broker
from FastTest import OrderExecutionType, OrderTargetType, PortfolioTracerType

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
        cross_dict = self.exchange.get_exchange_feature("FAST_ABOVE_SLOW")
        allocations = {id : val*100 for id,val in cross_dict.items()}
        self.portfolio1.order_target_allocations(
            allocations,
            "dummy",
            .01,
            order_target_type = OrderTargetType.UNITS
        )
        
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
        hal.register_strategy(strategy,"test")
        
        portfolio = hal.get_portfolio("test_portfolio1")
        portfolio.add_tracer(PortfolioTracerType.EVENT)

        hal.build()
        hal.run()
                
        cash_actual = np.array([100000, 100000,  90300, 100450, 100450,  90850.0,])
        nlv_actual = np.array([100000, 100000, 100000, 100450, 100450, 100450.0,])
        
        for portfolio in ["master", "test_portfolio1"]:
            portfolio = hal.get_portfolio(portfolio)
            cash_history = portfolio.get_tracer(PortfolioTracerType.VALUE).get_cash_history()
            nlv_history = portfolio.get_tracer(PortfolioTracerType.VALUE).get_nlv_history()
            
            assert(np.array_equal(cash_actual, cash_history))
            assert(np.array_equal(nlv_actual, nlv_history))
          
        hal.replay()
        
        cash_actual = np.array([100000, 100000,  90300, 100450, 100450,  90850.0,])
        nlv_actual = np.array([100000, 100000, 100000, 100450, 100450, 100450.0,])
        
        for portfolio in ["master", "test_portfolio1"]:
            portfolio = hal.get_portfolio(portfolio)
            cash_history = portfolio.get_tracer(PortfolioTracerType.VALUE).get_cash_history()
            nlv_history = portfolio.get_tracer(PortfolioTracerType.VALUE).get_nlv_history()
        
            assert(np.array_equal(nlv_actual, nlv_history))
            assert(np.array_equal(cash_actual, cash_history))
            
    def test_hal_reset(self):
        hal = helpers.create_simple_hal(logging=0)
        hydra = hal.get_hydra()
        portfolio = hal.new_portfolio("test_portfolio1",100000.0);
        
        hal.build()
        hydra.forward_pass()

        portfolio.place_market_order(
            helpers.test2_asset_id,
            100.0,
            "dummy",
            FastTest.OrderExecutionType.EAGER,
            -1
        )
        p1 = portfolio.get_position(helpers.test2_asset_id)
        assert(p1 is not None)
        assert(p1.get_units() == 100.0)
        assert(p1.get_average_price() == 101.0)
        
        hydra.on_open()
        hydra.backward_pass()
        hydra.forward_pass()
        
        hal.reset()
        
        p1 = portfolio.get_position(helpers.test2_asset_id)
        assert(p1 is None)
        
        hydra.forward_pass()
                
        portfolio.place_market_order(
            helpers.test2_asset_id,
            100.0,
            "dummy",
            FastTest.OrderExecutionType.EAGER,
            -1
        )
        
        p1 = portfolio.get_position(helpers.test2_asset_id)
        assert(p1 is not None)
        assert(p1.get_units() == 100.0)
        assert(p1.get_average_price() == 101.0)
        
    def test_hal_runto(self):
        hal = helpers.create_simple_hal(logging=0)
        hydra = hal.get_hydra()
        portfolio = hal.new_portfolio("test_portfolio1",100000.0);
        exchange = hal.get_exchange(helpers.test1_exchange_id)
        
        hal.build()
        hydra.forward_pass()

        portfolio.place_market_order(
            helpers.test2_asset_id,
            100.0,
            "dummy",
            FastTest.OrderExecutionType.EAGER,
            -1
        )
        p1 = portfolio.get_position(helpers.test2_asset_id)
        assert(p1 is not None)
        assert(p1.get_units() == 100.0)
        assert(p1.get_average_price() == 101.0)
        
        hydra.on_open()
        hydra.backward_pass()
        hal.run(to = "2000-06-08")
        
        mp = hal.get_portfolio("master")
        p_mp = mp.get_position(helpers.test2_asset_id)
        assert(p_mp.get_unrealized_pl() == 50)
        
        exchange_features = exchange.get_exchange_feature("CLOSE")
        assert(exchange_features[helpers.test2_asset_id] == 101.5)
        assert(exchange_features[helpers.test1_asset_id] == 105)
        
        hal.run()
        
        assert(p_mp.get_unrealized_pl() == -500)
        
    def test_hal_runto(self):
        hal = helpers.create_simple_hal(logging=0)
        hydra = hal.get_hydra()
        portfolio = hal.new_portfolio("test_portfolio1",100000.0);
        exchange = hal.get_exchange(helpers.test1_exchange_id)
        
        hal.build()
        hydra.forward_pass()

        portfolio.place_market_order(
            helpers.test2_asset_id,
            100.0,
            "dummy",
            FastTest.OrderExecutionType.EAGER,
            -1
        )
        p1 = portfolio.get_position(helpers.test2_asset_id)
        assert(p1 is not None)
        assert(p1.get_units() == 100.0)
        assert(p1.get_average_price() == 101.0)
        
        hydra.on_open()
        hydra.backward_pass()
        
        hal.run(steps = 3)
        
        mp = hal.get_portfolio("master")
        p_mp = mp.get_position(helpers.test2_asset_id)
        assert(p_mp.get_unrealized_pl() == 50)
        
        exchange_features = exchange.get_exchange_feature("CLOSE")
        assert(exchange_features[helpers.test2_asset_id] == 101.5)
        assert(exchange_features[helpers.test1_asset_id] == 105)
        
        hal.run()
        
        assert(p_mp.get_unrealized_pl() == -500)
        
    def test_hal_goto(self):
        hal = helpers.create_simple_hal(logging=0)
        hydra = hal.get_hydra()
        portfolio = hal.new_portfolio("test_portfolio1",100000.0);
        exchange = hal.get_exchange(helpers.test1_exchange_id)
        
        hal.build()
        hal.goto_datetime("2000-06-07")
        hydra.forward_pass()

        portfolio.place_market_order(
            helpers.test2_asset_id,
            100.0,
            "dummy",
            FastTest.OrderExecutionType.EAGER,
            -1
        )
        p1 = portfolio.get_position(helpers.test2_asset_id)
        assert(p1 is not None)
        assert(p1.get_units() == 100.0)
        assert(p1.get_average_price() == 98.0)
        
        
        hydra.on_open()
        
        exchange_features = exchange.get_exchange_feature("CLOSE")
        assert(exchange_features[helpers.test2_asset_id] == 97)
        assert(exchange_features[helpers.test1_asset_id] == 103)
        
        hydra.backward_pass()
        
        hal.run()
        
        mp = hal.get_portfolio("master")
        p_mp = mp.get_position(helpers.test2_asset_id)
        assert(p_mp.get_unrealized_pl() == -200)
        
        nlv_history = mp.get_tracer(PortfolioTracerType.VALUE).get_nlv_history()
        assert(np.array_equal(np.array([99900, 100350, 100350, 99800]),nlv_history))
        
    def test_hal_goto_multi(self):
        hal = helpers.create_simple_hal(logging=0)
        hydra = hal.get_hydra()
        portfolio = hal.new_portfolio("test_portfolio1",100000.0);
        exchange = hal.get_exchange(helpers.test1_exchange_id)
        
        hal.build()
        hydra.forward_pass()

        portfolio.place_market_order(
            helpers.test2_asset_id,
            100.0,
            "dummy",
            FastTest.OrderExecutionType.EAGER,
            -1
        )
        
        hydra.on_open()
        hydra.backward_pass()
        
        hal.run(to = "2000-06-08")
        hydra.forward_pass()
        
        portfolio.place_market_order(
            helpers.test2_asset_id,
            -100.0,
            "dummy",
            FastTest.OrderExecutionType.EAGER,
            -1
        )
        hydra.on_open()
        hydra.backward_pass()
        
        hal.run()
        mp = hal.get_portfolio("master")
        nlv_history = mp.get_tracer(PortfolioTracerType.VALUE).get_nlv_history()
        assert(np.array_equal(nlv_history,np.array([100050,  99800,  99600, 100050, 100000, 100000.0])))
    
    def test_hal_big(self):
        hal = helpers.create_big_hal(logging = 0, cash = 100000.0)
        exchange = hal.get_exchange(helpers.test1_exchange_id)
        mp = hal.get_portfolio("master")
        mp.add_tracer(PortfolioTracerType.EVENT)
        
        strategy = MovingAverageStrategy(hal)
        hal.register_strategy(strategy,"test")      
        hal.build()
        
        st = time.time()
        hal.run()
        et = time.time()
        
        event_tracer = mp.get_tracer(PortfolioTracerType.EVENT)
        assert(len(event_tracer.get_order_history()) > 0)
        
        execution_time = et - st
        candles = hal.get_candles()
        
        print(f"HAL: candles: {candles:.4f} candles")
        print(f"HAL: execution time: {execution_time:.4f} seconds")
        print(f"HAL: candles per second: {(candles / execution_time):,.3f}")     
        nlv1 = mp.get_tracer(PortfolioTracerType.VALUE).get_nlv_history()[-1]
        return
        st = time.time()
        hal.replay()
        et = time.time()
        
        execution_time = et - st
        candles = hal.get_candles()
        print("")
        print(f"HAL: candles: {candles:.4f} candles")
        print(f"HAL: execution time: {execution_time:.4f} seconds")
        print(f"HAL: candles per seoncd: {(candles / execution_time):,.3f}")   
        
        nlv2 = mp.get_tracer(PortfolioTracerType.VALUE).get_nlv_history()[-1]
        assert(nlv1==nlv2)
        
if __name__ == '__main__':
    unittest.main()
    #tests % python -m cProfile -s cumtime test_hal.py
    