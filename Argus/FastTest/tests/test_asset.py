
import sys
import os
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
        cpp_vol = spy.get_volatility()

        df = pd.read_csv(helpers.test_spy_file_path)
        df.set_index("Date", inplace=True)

        length = 252
        df["returns"] = df["Close"].pct_change()
        df["vol"] = df["Close"].pct_change().rolling(length).std(ddof=1)

        _sum = df["returns"].head(length).sum()
        mean = df["returns"].head(length).sum() / (length - 1)
        sos = (df["returns"].head(length)**2).sum()
        vol = ((sos / length) - mean**2)**(.5)

        assert((cpp_vol - vol) < 1e-10)

        hydra.build()
        
        hydra.forward_pass()
        hydra.on_open()
        hydra.backward_pass()
        
        # 2 because first is NAN
        df = df.iloc[2:]

        _sum = df["returns"].head(length).sum()
        mean = df["returns"].head(length).sum() / (length - 1)
        sos = (df["returns"].head(length)**2).sum()
        vol = ((sos / length) - mean**2)**(.5)

        cpp_vol = spy.get_volatility()
        assert((cpp_vol - vol) < 1e-10)
                
if __name__ == '__main__':
    unittest.main()
