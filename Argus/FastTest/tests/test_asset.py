
import sys
import os
import time
import unittest
import pandas as pd

sys.path.append(os.path.abspath('..'))
sys.path.append(os.path.abspath('../lib'))

import FastTest
from FastTest import AssetTracerType
from Hal import Hal, asset_from_df
import helpers


def test_gc_helper():
    df = helpers.load_df(helpers.test1_file_path, helpers.test1_asset_id)
    asset = asset_from_df(df, helpers.test1_asset_id)
    return asset

class AssetTestMethods(unittest.TestCase):
    def test_asset_load(self):
        asset1 = helpers.load_asset(
            helpers.test1_file_path,
            "asset1",
            helpers.test1_exchange_id,
            helpers.test1_broker_id
        )
        assert (True)

    def test_asset_get(self):
        asset1 = helpers.load_asset(
            helpers.test1_file_path,
            helpers.test1_exchange_id,
            helpers.test1_broker_id,
            "asset1"
        )
        assert (asset1.get("CLOSE", 0) == 101)
        assert (asset1.get("OPEN", 3) == 105)

    def test_asset_memory_address(self):
        asset1 = helpers.load_asset(
            helpers.test1_file_path,
            "asset1",
            helpers.test1_exchange_id,
            helpers.test1_broker_id
        )

        hydra = FastTest.new_hydra(0)
        exchange = hydra.new_exchange("exchange1")

        # register the existing asset to the exchange, the asset in the exchange's market
        # should have the same meory address as asset1 created above
        hydra.register_asset(asset1,"exchange1")
        asset2 = exchange.get_asset("asset1")

        address_1 = asset1.get_mem_address()
        address_2 = asset2.get_mem_address()

        assert (address_1 == address_2)

    def test_vol_tracer(self):
        hal = helpers.create_spy_hal()
        hydra = hal.get_hydra()
        spy = hydra.get_asset("SPY")

        assert(spy is not None)

        spy.add_tracer(AssetTracerType.VOLATILITY, 252, True)

        hydra.build()

        cpp_vol = spy.get_volatility()

        df = pd.read_csv(helpers.test_spy_file_path)
        df.set_index("Date", inplace=True)

        length = 252
        df["returns"] = df["Close"].pct_change()
        df.dropna(inplace = True)
        df["vol"] = df["returns"].rolling(length-1).var(ddof=0)

        vol_2 = df["returns"].values[0:length-1].var(ddof=0)

        vol = df["vol"].values[250]
        assert(abs(cpp_vol - vol) < 1e-6)
        
        hydra.forward_pass()
        hydra.on_open()
        hydra.backward_pass()

        vol = df["vol"].values[251]

        cpp_vol = spy.get_volatility()
        assert(abs(cpp_vol - vol) < 1e-6)

    def test_beta_tracer(self):
        hal = helpers.create_beta_hal(logging=0)
        hydra = hal.get_hydra()
        length = 252

        exchange = hydra.get_exchange(helpers.test1_exchange_id)
        exchange.add_tracer(AssetTracerType.BETA, length, True)

        hal.build()
        print("ok")

        market = exchange.get_market()
        spy = exchange.get_index_asset()
        dfs = {}
        for key, value in market.items():
            asset_df = hal.asset_to_df(value)
            asset_df_spy = hal.asset_to_df(spy)
            asset_df_spy.columns = [col + "_SPY" for col in asset_df_spy.columns]

            asset_df = pd.merge(asset_df, asset_df_spy["Close_SPY"], left_index=True, right_index=True, how = "left")
            asset_df["returns"] = asset_df["Close"].pct_change()
            asset_df["returns_SPY"] = asset_df["Close_SPY"].pct_change()

            length = 251
            rolling_covariance = asset_df["returns"].rolling(window=length).cov(asset_df["returns_SPY"], ddof=0)
            rolling_variance = asset_df["returns_SPY"].rolling(window=length).var(ddof = 0)
            asset_df["rolling_variance"] = rolling_variance
            asset_df["rolling_cov"] = rolling_covariance
            asset_df["BETA"] = rolling_covariance / rolling_variance
            asset_df.dropna(inplace = True)
            dfs[key] = asset_df
        
        for i in range(2):
            betas = {key : value.get_beta() for key, value in market.items()}
            for key, value in market.items():
                asset_df = dfs[key]
                assert(abs(betas[key] - asset_df["BETA"].values[0]) < 1e-4)

            hydra.forward_pass()
            hydra.on_open()
            hydra.backward_pass()

            betas = {key : value.get_beta() for key, value in market.items()}
            for key, value in market.items():
                asset_df = dfs[key]
                assert(abs(betas[key] - asset_df["BETA"].values[1]) < 1e-4)
            hal.reset()

if __name__ == '__main__':
    unittest.main()
