import time
import sys 
import os
import random
from decimal import Decimal, getcontext
from datetime import datetime

import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.ticker as mtick
import seaborn as sns

os.add_dll_directory("C:\\msys64\\mingw64\\bin")
sys.path.append(os.path.abspath('..'))
sys.path.append(os.path.abspath('../lib'))

from Hal import Hal
import FastTest
from FastTest import PortfolioTracerType

#import pybroker
#from pybroker.data import DataSource
#from pybroker import Strategy, StrategyConfig

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
        cross_dict = self.exchange.get_exchange_feature("ma_signal", -1)

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
                continue

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
    def notify_order(self, order):
        # Check if an order has been completed
        if order.status in [order.Completed]:
            print(
                f"{self.datetime.date()} "
                f"{order.data._name:<6} {('BUY' if order.isbuy() else 'SELL'):<5} "
                f"Price: {order.executed.price:6.4f} "
                f"Size: {order.created.size:9.4f} "
            )
                
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
            
def test_backtrader(dfs, log = False):
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
    
    thestrat = results[0]
    nlv = np.array(thestrat.observers.value.lines.value.array)
    
    
    cps_bt = candle_count / (et-st)
    if log:
        print(f"Backtrader loaded in {lt-st:.6f} seconds")
        print(f"Backtrader completed in {et-st:.6f} seconds")
        print(f"Backtrader candles per seconds: {cps_bt:,.2f}")   
        print(f"Backtrader Final Portfolio Value: {cerebro.broker.getvalue():,.4f}")
        print(f"Backtrader Initial Portfolio Value: {nlv[1]:,.4f}")

    return nlv, et-st


def test_fasttest(dfs, log = False):
    candle_count = 0
    
    for asset_id, df in dfs.items():
        candle_count += len(df)

    st = time.time()
    hal = Hal(logging=0, cash = 100000.0)
    broker = hal.new_broker("test",0.0)
    exchange = hal.new_exchange("test")
    
    portfolio = hal.get_portfolio("master")
    portfolio.add_tracer(PortfolioTracerType.EVENT)

    for asset_id, df in dfs.items():
        hal.register_asset_from_df(df, asset_id, "test", "test", warmup = 1) 
        
    strategy = MovingAverageStrategy(hal)
    hal.register_strategy(strategy, " test") 
    
    hal.build()
    lt = time.time()
    hal.run()
    et = time.time()
    
    cps_bt = candle_count / (et-st)
    nlv = portfolio.get_tracer(PortfolioTracerType.VALUE).get_nlv_history()
    cash = portfolio.get_tracer(PortfolioTracerType.VALUE).get_cash_history()
    
    if log:
        print(f"FastTest loaded in {lt-st:.6f} seconds")
        print(f"FastTest completed in {et-st:.6f} seconds")
        print(f"FastTest candles per seconds: {cps_bt:,.2f}")  
        print(f"FastTest Final Portfolio Value: {nlv[-1]:,.4f}")
        print(f"FastTest Initial Portfolio Value: {nlv[1]:,.4f}")

    orders = hal.get_order_history()        
    return nlv, cash, et-st, orders 
 
def test_fp_error():
    step_count = 203
    
    n_trials = 5
    counts = [1,10,20,30,60,100,150,200,250,350,450,500]
    ft_errors = []
    bt_errors = []
    
    getcontext().prec = 50
    for count in counts:
        ft_error = 0
        bt_error = 0
        for i in range(n_trials):
            dfs = load_data(count, step_count)
            
            ft_nlv, ft_cash, ft_time, orders = test_fasttest(dfs)
            bt_nlv, bt_time = test_backtrader(dfs)
            
            pls = {asset_id  : dfs[asset_id]["Open"].diff().values[2] for asset_id in list(dfs.keys())}
            df_pls = pd.DataFrame(pls.items(), columns = ["asset_id","pl"])
                        
            orders = pd.merge(orders, df_pls, how = "left", on = "asset_id")
            orders['pl'] = orders['pl'].apply(lambda x: Decimal(str(x)))
            orders['units'] = orders['units'].apply(lambda x: Decimal(str(x)))
            sum_product = (orders['units'] * orders['pl']).sum()
            
            true_nlv =  sum_product + Decimal("100000")
                        
            ft_error += (true_nlv - Decimal(ft_nlv[1]))
            bt_error += (true_nlv - Decimal(bt_nlv[1]))
            
        ft_errors.append((ft_error / n_trials)/ 100000)
        bt_errors.append((bt_error / n_trials)/ 100000)
        print(f"{count} Asset, FastTest Absolute Error: {ft_errors[-1]:.7f}$, Backtrader Abolute Error: {bt_errors[-1]:.7f}$")
        
    fig, ax1 = plt.subplots()
    ax1.plot(counts,bt_errors, alpha = .5, label = "Backtrader")
    ax1.plot(counts,ft_errors, alpha = .5, label = "FastTest")
    ax1.yaxis.set_major_formatter(mtick.PercentFormatter())
    ax1.legend()
    plt.ylabel('error (pct of total nlv)')
    plt.ylabel('# Assets')
    plt.title("One Step Floating Point Error")
    plt.show()
   
"""
def test_pybroker(dfs):
    config = StrategyConfig(initial_cash=100_000)
    pybroker.register_columns('ma_signal')
    df = dfs["asset0"].copy()
    df["symbol"] = "asset0"
    df["high"] = df["Open"]
    df["low"] = df["Open"]
    df["Close"] = df["Open"]
    df.reset_index(inplace = True)
    df.rename(columns={"Open" : "open",'Close': 'close', "index" : "date"}, inplace=True)
    
    def ma_cross(ctx):
        pos_long = ctx.long_pos()
        pos_short = ctx.short_pos()
        
        if not pos_long and ctx.ma_signal[-1] == 1:
            ctx.buy_shares = 100
        elif pos_long and ctx.ma_signal[-1] == 0:
            ctx.sell_shares = 200
        elif not pos_short and ctx.ma_signal[-1] == 0:
            ctx.sell_shares = 100
        elif pos_short and ctx.ma_signal[-1] == 1:
            ctx.buy_shares = 200  
        
    
    strategy = Strategy(df, df["date"].values[0].astype(str), df["date"].values[-1].astype(str))
    strategy.add_execution(ma_cross, ['asset0'])
    result = strategy.backtest()
    print(result.orders)
    
    hal = Hal(logging=0, cash = 100000.0)
    broker = hal.new_broker("test",0.0)
    exchange = hal.new_exchange("test")
    
    portfolio = hal.get_portfolio("master")
    portfolio.add_tracer(PortfolioTracerType.EVENT)

    for asset_id, df in dfs.items():
        if asset_id != "asset0":
            continue
        hal.register_asset_from_df(df, asset_id, "test", "test", warmup = 1) 
        
    strategy = MovingAverageStrategy(hal)
    hal.register_strategy(strategy, " test") 
    
    hal.build()
    lt = time.time()
    hal.run()
    et = time.time()
    
    nlv_ft = portfolio.get_tracer(PortfolioTracerType.VALUE).get_nlv_history()
    cash = portfolio.get_tracer(PortfolioTracerType.VALUE).get_cash_history()
    
    nlv_pyb = result.portfolio['market_value'].values
    print(result.portfolio)
    print(nlv_pyb, nlv_ft)
    
    orders = hal.get_order_history() 
    print(orders)
"""
    
if __name__ == "__main__":
    count = 1
    step_count = 500
    
    #test_fp_error()
    
    dfs = load_data(count, step_count)
    
    #test_pybroker(dfs)
    
    #print(f"{count * step_count:,} candles loaded\n")
    #print()
                
    ft_nlv, ft_cash, ft_time, orders = test_fasttest(dfs, log=False)
    #print()
    bt_nlv, bt_time = test_backtrader(dfs,log = False)
    #print()
    
    print("FastTest orders:")
    print(orders)

    bt_nlv = bt_nlv[0:len(ft_nlv)]    
    print(ft_nlv[-1], bt_nlv[-1])
    
    #print(f"fastest \033[32m{bt_time / ft_time:.4}x\033[0m faster")
        
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(10, 8))
    ax1.plot(bt_nlv, alpha = .5, label = "Backtrader")
    ax1.plot(ft_nlv, alpha = .5, label = "FastTest")
    ax1.legend()
    
    ax2.plot(ft_nlv - bt_nlv, alpha = .5, color = "black", label = "epsilon")
    ax2.legend()
    
    fig.suptitle(f"{count} Assets with {step_count - 200} rows", fontsize=14, fontweight='bold')
    plt.tight_layout()
    plt.show()

