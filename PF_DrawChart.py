"""
    Make this code into a module so it can be loaded at program
    startup and parsed just once.

    This module is called from a C++ program using an embedded Python
    interpreter.  The C++ program creates a set of local variables which 
    are made available to this code.
"""

"""
	/* This file is part of PF_CollectData. */

	/* PF_CollectData is free software: you can redistribute it and/or modify */
	/* it under the terms of the GNU General Public License as published by */
	/* the Free Software Foundation, either version 3 of the License, or */
	/* (at your option) any later version. */

	/* PF_CollectData is distributed in the hope that it will be useful, */
	/* but WITHOUT ANY WARRANTY; without even the implied warranty of */
	/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the */
	/* GNU General Public License for more details. */

	/* You should have received a copy of the GNU General Public License */
	/* along with PF_CollectData.  If not, see <http://www.gnu.org/licenses/>. */


use axline to plot 45 degree trend lines

"""

import pandas as pd 
import matplotlib
import matplotlib.pyplot as plt
import mplfinance as mpf
from matplotlib.ticker import ScalarFormatter
import numpy as np

# load a non-GUI back end since we only produce graphic files as output

matplotlib.use("SVG")

SLOPE = 0.707

# some functions to use for trend lines 


def FindMinimum(is_up, chart_data):
    # we expect a data_frame 

    minimum = 100000
    minimum_column = 0

    for i in range(len(is_up)):
        if not is_up[i]:
            # a down column
            if chart_data.iloc[i]["Low"] < minimum:
                minimum = chart_data.iloc[i]["Low"]
                minimum_column = i

    return minimum_column


def FindNextMaximum(data, start_at):
    pass


def SetStepbackColor (is_up, stepped_back) : 
    if is_up:
        if stepped_back:
            return "blue"
    if not is_up:
        if stepped_back:
            return "orange"
    return None

def DrawChart(the_data, IsUp, StepBack, ChartTitle, ChartFileName, DateTimeFormat, ShowTrendLines, UseLogScale, Y_min, Y_max, openning_price):

    chart_data = pd.DataFrame(the_data)
    chart_data["Date"] = pd.to_datetime(chart_data["Date"])
    chart_data.set_index("Date", drop=False, inplace=True)

    chart_data["row_number"] = np.arange(chart_data.shape[0])

    mco = []
    for i in range(len(IsUp)):
        mco.append(SetStepbackColor(IsUp[i], StepBack[i]))

    mc = mpf.make_marketcolors(up='g',down='r')
    s  = mpf.make_mpf_style(marketcolors=mc, gridstyle="dashed")

    # now generate a sequence of date pairs:
    # dates = chart_data["Date"]
    # datepairs = [(d1,d2) for d1,d2 in zip(dates,dates[1:])]

    if chart_data.shape[0] < 3:
        ShowTrendLines = "no"

    if ShowTrendLines == "no":

        fig, axlist = mpf.plot(chart_data,
            type="candle",
            style=s,
            marketcolor_overrides=mco,
            title=ChartTitle,
            figsize=(14, 10),
            datetime_format=DateTimeFormat,
            hlines=dict(hlines=[openning_price],colors=['r'],linestyle='dotted',linewidths=(2)),
            returnfig=True)

    elif ShowTrendLines == "angle":

        # 45 degree trend line.
        # need 2 points: first down column bottom and computed value for last column 

        x1 = FindMinimum(IsUp, chart_data)
        y1 = chart_data.iloc[x1]["Low"]
        x2 = chart_data.shape[0] - 1

        # formula for point slope line equation
        # y = slope(x - x1) + y1

        y2 = SLOPE * (x2 - x1) + y1
        a_line_points = [(chart_data.iloc[x1]["Date"], y1), (chart_data.iloc[x2]["Date"], y2)]
        fig, axlist = mpf.plot(chart_data,
            type="candle",
            style=s,
            marketcolor_overrides=mco,
            title=ChartTitle,
            figsize=(14, 10),
            datetime_format=DateTimeFormat,
            alines=a_line_points,
            returnfig=True)

    else:

        d1 = chart_data.index[ 0]
        d2 = chart_data.index[-1]
        tdates = [(d1,d2)]
    
        fig, axlist = mpf.plot(chart_data,
            type="candle",
            style=s,
            marketcolor_overrides=mco,
            title=ChartTitle,
            figsize=(14, 10),
            datetime_format=DateTimeFormat,
            tlines=[dict(tlines=tdates,tline_use='High',tline_method="point-to-point",colors='r'),
                dict(tlines=tdates,tline_use='Low',tline_method="point-to-point",colors='b')],
            returnfig=True)

    axlist[0].tick_params(which='both', left=True, right=True, labelright=True)
    axlist[0].secondary_xaxis('top')

    if UseLogScale:
        plt.ylim(Y_min, Y_max)
        axlist[0].set_yscale("log")
        axlist[0].grid(which='both', axis='both', ls='-')
        axlist[0].yaxis.set_major_formatter(ScalarFormatter())
        axlist[0].yaxis.set_minor_formatter(ScalarFormatter()) 

    plt.savefig(ChartFileName)
    for ax in axlist:
        ax.clear()
        del(ax)
    plt.close(fig)
    del(axlist)
    del(fig)
