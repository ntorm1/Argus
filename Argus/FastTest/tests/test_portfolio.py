import sys
import os
import time
import unittest
import numpy as np

sys.path.append(os.path.abspath('..'))
sys.path.append(os.path.abspath('../lib'))

import FastTest
from FastTest import OrderExecutionType, OrderTargetType, PortfolioTracerType

import helpers

class PortfolioTestMethods(unittest.TestCase):
    def test_mp_new_portfolio(self):
        print("test_mp_new_portfolio running...")
        hydra = FastTest.new_hydra(0)
        portfolio = hydra.new_portfolio("test_portfolio",100.0);
        print("test_mp_new_portfolio passed...")
        assert(True)
        
    def test_sub_portfolio_create(self):
        print("test_sub_portfolio_create running...")
        hydra = FastTest.new_hydra(1)
        
        portfolio1 = hydra.new_portfolio("test_portfolio1",100.0);
        portfolio2 = hydra.new_portfolio("test_portfolio2",100.0);  
        portfolio3 = portfolio1.create_sub_portfolio("test_portfolio3",100.0);
        
        mp = hydra.get_master_portfolio();
        portfolio1_search = mp.find_portfolio("test_portfolio1");
                
        address_1 = portfolio1.get_mem_address()
        address_2 = portfolio1_search.get_mem_address()
        
        assert(address_1 == address_2)

        portfolio3_search_mp = mp.find_portfolio("test_portfolio3");
        portfolio3_search_1 = portfolio1.find_portfolio("test_portfolio3");
        
        assert(portfolio3.get_mem_address() == portfolio3_search_mp.get_mem_address() == portfolio3_search_1.get_mem_address())
        print("test_sub_portfolio_create passed...")

    def test_portfolio_order_prop(self):
        print("test_portfolio_order_prop running...")

        hydra = helpers.create_simple_hydra(logging=0)
        mp = hydra.get_master_portfolio()
        
        portfolio1 = hydra.new_portfolio("test_portfolio1",100.0);
        portfolio2 = hydra.new_portfolio("test_portfolio2",100.0);  
        portfolio3 = portfolio1.create_sub_portfolio("test_portfolio3",100.0);
        
        hydra.build()
        hydra.forward_pass()

        portfolio2.place_market_order(
            helpers.test2_asset_id,
            100.0,
            "dummy",
            FastTest.OrderExecutionType.EAGER,
            -1
        )
        
        p1 = portfolio1.get_position(helpers.test2_asset_id)
        p2 = portfolio2.get_position(helpers.test2_asset_id)
        p_mp = mp.get_position(helpers.test2_asset_id)

        assert(p2 is not None)
        assert(p_mp is not None)    
        assert(portfolio3.get_position(helpers.test2_asset_id) is None)
        assert(portfolio1.get_position(helpers.test2_asset_id) is None)
        
        assert(p2.get_units() == 100.0)
        assert(p2.get_average_price() == 101.0)
        assert(p1 is None)
        
        #static trade counter
        #trade1 = p2.get_trade(0)
        #trade_mp = p_mp.get_trade(0)
        
        ##assert(trade1 is not None)
        #assert(trade_mp is not None)    
        #assert(trade1.get_mem_address() == trade_mp.get_mem_address())
        #assert(trade1.get_units() == 100.0)
        #assert(trade1.get_average_price() == 101.0)
        print("test_sub_portfolio_create passed...")

    def test_portfolio_order_increase(self):
        print("test_portfolio_order_increase running...")

        hydra = helpers.create_simple_hydra(logging=0)
        mp = hydra.get_master_portfolio()
        
        portfolio1 = hydra.new_portfolio("test_portfolio1",100.0);
        portfolio2 = hydra.new_portfolio("test_portfolio2",100.0);  
        
        hydra.build()
        hydra.forward_pass()
        
        portfolio2.place_market_order(
            helpers.test2_asset_id,
            100.0,
            "dummy",
            FastTest.OrderExecutionType.EAGER,
            -1
        )
        portfolio1.place_market_order(
            helpers.test2_asset_id,
            50.0,
            "dummy",
            FastTest.OrderExecutionType.EAGER,
            -1
        )
        
        p_mp = mp.get_position(helpers.test2_asset_id) 
        p1 = portfolio1.get_position(helpers.test2_asset_id)
        p2 = portfolio2.get_position(helpers.test2_asset_id)
        assert(p1.get_units() == 50.0)
        assert(p2.get_units() == 100.0)
        assert(p_mp.get_units() == 150.0)
        
        
        portfolio2.place_market_order(
            helpers.test2_asset_id,
            -100.0,
            "dummy",
            FastTest.OrderExecutionType.EAGER,
            -1
        )
        
        assert(portfolio2.get_position(helpers.test2_asset_id) is None)
        assert(not p2.is_open)
        assert(p_mp.get_units() == 50)
        assert(p1.get_units() == 50)
        print("test_portfolio_order_increase passed...")
        
    def test_portfolio_eval(self):
        print("test_portfolio_eval running...")

        hydra = helpers.create_simple_hydra(logging=0)
        mp = hydra.get_master_portfolio()
        
        portfolio1 = hydra.new_portfolio("test_portfolio1",10000.0);
        portfolio2 = hydra.new_portfolio("test_portfolio2",10000.0);  
        
        hydra.build()
        hydra.forward_pass()
        
        portfolio2.place_market_order(
            helpers.test2_asset_id,
            -100.0,
            "dummy",
            FastTest.OrderExecutionType.EAGER,
            -1
        )
        portfolio1.place_market_order( 
            helpers.test2_asset_id,
            50.0,
            "dummy",
            FastTest.OrderExecutionType.EAGER,
            -1
        )
        
        hydra.on_open()
        hydra.backward_pass()
        
        p0 = mp.get_position(helpers.test2_asset_id)
        p1 = portfolio1.get_position(helpers.test2_asset_id)
        p2 = portfolio2.get_position(helpers.test2_asset_id)
        
        assert(p0.get_units() == -50.0)
        assert(p1.get_units() == 50.0)
        assert(p2.get_units() == -100.0)
        
        assert(p0.get_unrealized_pl() == (-50.0 * .5))
            
        assert(mp.get_unrealized_pl() == (-50.0 * .5))
        assert(portfolio1.get_unrealized_pl() == (50.0 * .5))
        assert(portfolio2.get_unrealized_pl() == (-100.0 * .5))
        
        assert(portfolio1.get_cash() == (10000 - (50 * 101)))
        assert(portfolio2.get_cash() == (10000 - (-100 * 101)))
        assert(mp.get_cash() == portfolio1.get_cash() + portfolio2.get_cash())
        
        assert(portfolio1.get_nlv() == (10000 + (50 * .5)))
        assert(portfolio2.get_nlv() == (10000 + (-100 * .5)))
        assert(mp.get_nlv() == portfolio1.get_nlv() + portfolio2.get_nlv())
        print("test_portfolio_eval passed...")
        
            
    def test_portfolio_order_traget_size(self):
        print("test_portfolio_order_traget_size running...")

        hydra = helpers.create_simple_hydra(logging=0)
        mp = hydra.get_master_portfolio()
        
        portfolio1 = hydra.new_portfolio("test_portfolio1",10000.0);
        portfolio2 = hydra.new_portfolio("test_portfolio2",10000.0);  
        
        hydra.build()
        hydra.forward_pass()
        
        portfolio2.order_target_size(helpers.test2_asset_id, .01, "dummy", 
                .01,
                FastTest.OrderTargetType.PCT,
                FastTest.OrderExecutionType.EAGER,
                -1)
        p2 = portfolio2.get_position(helpers.test2_asset_id)     
        assert(p2 is not None)
        assert(p2.units == (10000 * .01)/ 101)
        
        portfolio1.order_target_size(helpers.test2_asset_id, 10, "dummy", 
                .01,
                FastTest.OrderTargetType.DOLLARS,
                FastTest.OrderExecutionType.LAZY,
                -1)
        
        p1 = portfolio1.get_position(helpers.test2_asset_id)        
        assert(p1 is None) # lazy exeution
        hydra.on_open()
        p1 = portfolio1.get_position(helpers.test2_asset_id)     
        assert(p1 is not None)
        assert(p1.units == 10 / 101)
        print("test_portfolio_order_traget_size passed...")
            
    def test_portfolio_close_position(self):
        print("test_portfolio_close_position running...")

        hydra = helpers.create_simple_hydra(logging=0)
        mp = hydra.get_master_portfolio()
        
        portfolio1 = hydra.new_portfolio("test_portfolio1",100.0);
        
        hydra.build()
        hydra.forward_pass()

        portfolio1.place_market_order(
            helpers.test2_asset_id,
            100.0,
            "dummy",
            FastTest.OrderExecutionType.EAGER,
            -1
        )
        
        p1 = portfolio1.get_position(helpers.test2_asset_id)
        assert(p1 is not None)
        portfolio1.close_position(helpers.test2_asset_id)
        p1 = portfolio1.get_position(helpers.test2_asset_id)
        assert(p1 is  None)
        
        hydra.on_open()
        hydra.backward_pass()
        
        hydra.forward_pass()
        
        portfolio1.place_market_order(
            helpers.test2_asset_id,
            100.0,
            "dummy",
            FastTest.OrderExecutionType.EAGER,
            -1
        )
        portfolio1.place_market_order(
            helpers.test1_asset_id,
            100.0,
            "dummy",
            FastTest.OrderExecutionType.EAGER,
            -1
        )
        
        p1 = portfolio1.get_position(helpers.test2_asset_id)
        p2 = portfolio1.get_position(helpers.test1_asset_id)
        assert(p1 is not None)
        assert(p2 is not None)
        
        portfolio1.close_position()
        p1 = portfolio1.get_position(helpers.test2_asset_id)
        p2 = portfolio1.get_position(helpers.test1_asset_id)
        assert(p1 is None)
        assert(p2 is None)
        print("test_portfolio_close_position passed...")
        
    def test_parent_portfolio_close(self):
        hydra = helpers.create_simple_hydra(logging=0)
        mp = hydra.get_master_portfolio()
        
        portfolio1 = hydra.new_portfolio("test_portfolio1",100000.0);
        portfolio2 = hydra.new_portfolio("test_portfolio2",100000.0);
        
        hydra.build()
        hydra.forward_pass()
        
        portfolio1.place_market_order(
            helpers.test2_asset_id,
            100.0,
            "dummy",
            FastTest.OrderExecutionType.EAGER,
            -1
        )
        
        portfolio2.place_market_order(
            helpers.test2_asset_id,
            -50.0,
            "dummy",
            FastTest.OrderExecutionType.EAGER,
            -1
        )
        
        mp.place_market_order(
            helpers.test2_asset_id,
            -50.0,
            "dummy",
            FastTest.OrderExecutionType.EAGER,
            -1
        )
        
        mp = mp.get_position(helpers.test2_asset_id)
        p1 = portfolio1.get_position(helpers.test2_asset_id)
        p2 = portfolio2.get_position(helpers.test2_asset_id)
        assert(mp is None)
        assert(p1 is None)
        assert(p2 is None)
        print("test_parent_portfolio_close passed...")
        
    def test_portfolio_target_allocations_order(self):
        print("test_portfolio_target_allocations_order running...")
        hydra = helpers.create_simple_hydra(logging=0)
        mp = hydra.get_master_portfolio()
        
        portfolio1 = hydra.new_portfolio("test_portfolio1",100000.0);
        portfolio2 = hydra.new_portfolio("test_portfolio2",100000.0);
        
        hydra.build()
        hydra.forward_pass()
        hydra.on_open()
        hydra.backward_pass()
        
        hydra.forward_pass()
        
        allocations = {helpers.test1_asset_id : 100, helpers.test2_asset_id : -100}
        portfolio1.order_target_allocations(
            allocations,
            "dummy",
            .01,
            order_target_type = OrderTargetType.UNITS
        )
        
        allocations = {helpers.test1_asset_id : -50, helpers.test2_asset_id : 50}
        portfolio2.order_target_allocations(
            allocations,
            "dummy",
            .01,
            order_target_type = OrderTargetType.UNITS
        )
        
        hydra.on_open()
        hydra.backward_pass()
        
        pos = portfolio1.get_position(helpers.test1_asset_id)
        assert(pos.get_nlv() == 100 * 101)
        pos = portfolio1.get_position(helpers.test2_asset_id)
        assert(pos.get_nlv() == -100 * 99)
        
        pos = portfolio2.get_position(helpers.test1_asset_id)
        assert(pos.get_nlv() == -50 * 101)
        pos = portfolio2.get_position(helpers.test2_asset_id)
        assert(pos.get_nlv() == 50 * 99)
        
        pos = mp.get_position(helpers.test1_asset_id)
        assert(pos.get_nlv() == 50 * 101)
        pos = mp.get_position(helpers.test2_asset_id)
        assert(pos.get_nlv() == -50 * 99)
                
        portfolio1_nlv1 =  100000 + (100 * (101-100)) + (-100 * (99 - 100))
        portfolio2_nlv1 =  100000 + (-50 * (101-100)) + (50 * (99 - 100))
        mp_nlv1 = 200000 + (50 * (101-100)) + (-50 * (99 - 100))
        assert(portfolio1.get_nlv() == portfolio1_nlv1)
        assert(portfolio2.get_nlv() == portfolio2_nlv1)
        assert(mp.get_nlv() == mp_nlv1)
        
        hydra.forward_pass()
        allocations = {helpers.test1_asset_id : -100, helpers.test2_asset_id : 100}
        portfolio1.order_target_allocations(
            allocations,
            "dummy",
            .01,
            order_target_type = OrderTargetType.UNITS
        )
        
        allocations = {helpers.test1_asset_id : 50, helpers.test2_asset_id : -50}
        portfolio2.order_target_allocations(
            allocations,
            "dummy",
            .01,
            order_target_type = OrderTargetType.UNITS
        )
        print()
        hydra.on_open()
        hydra.backward_pass()
        
        p1 = portfolio2.get_position(helpers.test1_asset_id)
        p2 = portfolio2.get_position(helpers.test2_asset_id)
        assert(p1.units == 50)
        assert(p2.units == -50)
        
        p1 = portfolio1.get_position(helpers.test1_asset_id)
        p2 = portfolio1.get_position(helpers.test2_asset_id)
        assert(p1.units == -100)
        assert(p2.units == 100)
                
        portfolio1_nlv2 =  portfolio1_nlv1 + (-100 * (98-99)) + (100 * (102 - 101)) + (100 * (97-98)) + (-100 * (103 - 102))
        assert(portfolio1_nlv2 == portfolio1.get_nlv())
        
        portfolio2_nlv2 =  portfolio2_nlv1 + (50 * (98-99)) + (-50 * (102 - 101)) + (-50 * (97-98)) + (50 * (103 - 102))
        assert(portfolio2_nlv2 == portfolio2.get_nlv())
        assert(mp.get_nlv() == portfolio1_nlv2 + portfolio2_nlv2)
        print("test_portfolio_target_allocations_order passed...")
                    
    def test_portfolio_target_allocations_short(self):
        print("test_portfolio_target_allocations_short running...")
        hydra = helpers.create_simple_hydra(logging=0)
        portfolio1 = hydra.new_portfolio("test_portfolio1",100000.0);

        hydra.build()
        
        hydra.forward_pass()
        hydra.on_open()
        hydra.backward_pass()
        
        hydra.forward_pass()
        hydra.on_open()
        
        allocations = {helpers.test1_asset_id : -.4, helpers.test2_asset_id : .6}
        portfolio1.order_target_allocations(
            allocations,
            "dummy",
            .01,
        )
        hydra.backward_pass()
        
        p1 = portfolio1.get_position(helpers.test1_asset_id)
        p2 = portfolio1.get_position(helpers.test2_asset_id)
        assert(p1.units == -1*(100000 * .4)/101)
        assert(p2.units == (100000 * .6)/99)
        assert(p1.average_price == 101)
        assert(p2.average_price == 99)
        
        
        
        hydra.forward_pass()
        hydra.on_open()
        
        mp = hydra.get_portfolio("master")
        p1 = mp.get_position(helpers.test1_asset_id)
        p2 = mp.get_position(helpers.test2_asset_id)
        assert(p1.get_last_price() == 103)
        assert(p2.get_last_price() == 97)
        
        nlv = 100000 + ((100000 * .6)/99)*(97-99) + (-1*(100000 * .4)/101)*(103-101)
        assert(abs(nlv == portfolio1.get_nlv()))
        print("test_portfolio_target_allocations_short passed...")

                
        """
        allocations = {helpers.test1_asset_id : -.4, helpers.test2_asset_id : .6}
        portfolio1.order_target_allocations(
            allocations,
            "dummy",
            .01,
        )
        hydra.backward_pass()
        """
                    

if __name__ == '__main__':  
    unittest.main()
