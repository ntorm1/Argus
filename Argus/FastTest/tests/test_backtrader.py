import time
import sys 
import os
from datetime import datetime

import numpy as np
import pandas as pd
import matplotlib.pyplot as plt

sys.path.append(os.path.abspath('..'))

from Hal import Hal
import FastTest

import backtrader as bt
import backtrader.feeds as btfeeds
import backtrader.indicators as btind

class CustomDataLoader(bt.feeds.PandasData):
    lines = ('ma_signal',)

    params = (
        ('datetime',None),
        ("open", "Open"),
        ('close', "Close"),
        ('ma_signal', "ma_signal"),
        ('high', None),
        ('low', None),
        ('volume', None),
        ('openinterest', None)
    )
    
def load_data(count, step_count):    
    dfs = {}
    step_count = step_count
    for i in range(count):
        steps = np.random.uniform(-1, 1, step_count)
        noise = np.random.uniform(0, 1, step_count)
        df = pd.DataFrame(
            data = 100 + np.cumsum(steps),
            columns = ["Close"],
            index = pd.date_range(end='1/1/2020', periods=step_count, freq = "s")
        )
        df["Open"] = df["Close"] + noise
        df["ma_signal"] = (df["Close"].rolling(50).mean() > df["Close"].rolling(200).mean()).astype(float)
        df = df.iloc[200:]
        df = df.round(5)
        dfs["asset" + str(i)] = df
    return dfs

class MovingAverageStrategy:
    def __init__(self, hal : Hal) -> None:        
        self.exchange = hal.get_exchange("test")
        self.broker = hal.get_broker("test")
        self.portfolio = hal.get_portfolio("master");
                
    def build(self) -> None:
        return
            
    def on_close(self) -> None:
        return
    
    def on_open(self) -> None:
        
        cross_dict = {}
        self.exchange.get_exchange_feature(cross_dict, "ma_signal", -1)

        for asset_id, cross_value in cross_dict.items():
            position = self.portfolio.get_position(asset_id)
            if position is None:
                if cross_value == 1:
                    units = 100
                else:
                    units = -100
                self.portfolio.place_market_order(
                    asset_id,
                    units,
                    "dummy")
            else:
                if cross_value == 1:
                    if position.units > 0:
                        continue
                    else:
                        self.portfolio.place_market_order(
                            asset_id,
                            200,
                            "dummy")
                else:
                    if position.units < 0:
                        continue
                    else:
                        self.portfolio.place_market_order(
                            asset_id,
                            -200,
                            "dummy",
                            FastTest.OrderExecutionType.EAGER,
                            -1)
                           
class BT_MA_Cross_Strategy(bt.Strategy):
    def __init__(self) -> None:
        super().__init__()
                
    def next(self):
        ma_signal = {d : d.ma_signal[0] for d in self.datas}
        
        for d, ma_signal in ma_signal.items():
            #print(self.datetime.date(), ma_signal)
            if not self.getposition(d):
                if ma_signal == 0:
                    self.sell(data=d, size = 100)
                else:
                    self.buy(data=d, size = 100)
                                    
            else:
                #long position
                if ma_signal == 1:
                    if self.getposition(d).size > 0:
                        continue
                    else:
                        self.buy(data=d, size = 200)
                #short position
                else:
                    if self.getposition(d).size < 0:
                        continue
                    else:
                        self.sell(data=d, size = 200)
            
def test_backtrader(dfs):
    candle_count = 0
    for asset_id, df in dfs.items():
        candle_count += len(df)
    
    st = time.time()
    cerebro = bt.Cerebro()
    cerebro.addstrategy(BT_MA_Cross_Strategy)   
    
    cerebro.broker.setcash(100000.0)
    cerebro.addobserver(bt.observers.Broker)
    cerebro.addobserver(bt.observers.Trades)
    cerebro.addobserver(bt.observers.BuySell)
    cerebro.addobserver(bt.observers.Value)
    
    for asset_id, df in dfs.items():
        data = CustomDataLoader(
            dataname = df,
            timeframe=bt.TimeFrame.Seconds
        )
        cerebro.adddata(data, name=asset_id)
    
    lt = time.time()
    results = cerebro.run()
    et = time.time()
    
    cps_bt = candle_count / (et-st)
    print(f"Backtrader loaded in {lt-st:.6f} seconds")
    print(f"Backtrader completed in {et-st:.6f} seconds")
    print(f"Backtrader candles per seconds: {cps_bt:,.2f}")   
    print(f"Backtrader Final Portfolio Value: {cerebro.broker.getvalue():,.4f}")
    
    thestrat = results[0]
    nlv = np.array(thestrat.observers.value.lines.value.array)
    return nlv, et-st


def test_fasttest(dfs):
    candle_count = 0
    
    for asset_id, df in dfs.items():
        candle_count += len(df)

    st = time.time()
    hal = Hal(logging=0, cash = 100000.0)
    broker = hal.new_broker("test",0.0)
    exchange = hal.new_exchange("test")
    
    for asset_id, df in dfs.items():
        hal.register_asset_from_df(df, asset_id, "test", "test", warmup = 1) 
        
    strategy = MovingAverageStrategy(hal)
    hal.register_strategy(strategy) 
    
    hal.build()
    lt = time.time()
    hal.run()
    et = time.time()
    
    cps_bt = candle_count / (et-st)
    portfolio_history = hal.get_portfolio("master").get_portfolio_history()
    nlv = portfolio_history.get_nlv_history()
        
    print(f"FastTest loaded in {lt-st:.6f} seconds")
    print(f"FastTest completed in {et-st:.6f} seconds")
    print(f"FastTest candles per seconds: {cps_bt:,.2f}")  
    print(f"FastTest Final Portfolio Value: {nlv[-1]:,.4f}")
        
    return nlv, et-st
 
if __name__ == "__main__":
    count = 10
    step_count = 10000
    dfs = load_data(count, step_count)
    print(f"{count * step_count:,} candles loaded\n")
    print()
    
    ft_nlv, ft_time = test_fasttest(dfs)
    print()
    bt_nlv, bt_time = test_backtrader(dfs)
    print()

    
    print(f"fastest \033[32m{bt_time / ft_time:.4}x\033[0m faster")
    
    fig, ax1 = plt.subplots()
    ax2 = ax1.twinx()
    ax1.plot(bt_nlv, alpha = .5, label = "Backtrader")
    ax1.plot(ft_nlv, alpha = .5, label = "FastTest")
    ax1.legend()
    plt.show()