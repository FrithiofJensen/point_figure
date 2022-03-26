// =====================================================================================
// 
//       Filename:  PF_CollectDataApp.h
// 
//    Description:  Module to collect Point & Figure data for a given set of symbols.
// 
//        Version:  2.0
//        Created:  2021-07-20 11:50 AM
//       Revision:  none
//       Compiler:  g++
// 
//         Author:  David P. Riedel (dpr), driedel@cox.net
//        License:  GNU General Public License v3
//        Company:  
// 
// =====================================================================================


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

#ifndef  PF_COLLECTDATAAPP_INC
#define  PF_COLLECTDATAAPP_INC


#include <filesystem>
//#include <map>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <tuple>
#include <utility>

//namespace fs = std::filesystem;

#include <boost/program_options.hpp>

namespace po = boost::program_options;

#include <date/date.h>
#include <spdlog/spdlog.h>

#include "Boxes.h"
#include "DDecQuad.h"
#include "PF_Chart.h"
#include "Tiingo.h"


// =====================================================================================
//        Class:  PF_CollectDataApp
//  Description:  application specific stuff
// =====================================================================================
class PF_CollectDataApp
{
public:

    using PF_Data = std::vector<std::pair<std::string, PF_Chart>>;

    // ====================  LIFECYCLE     =======================================
    PF_CollectDataApp (int argc, char* argv[]);                             // constructor

    // use ctor below for testing with predefined options

    explicit PF_CollectDataApp(const std::vector<std::string>& tokens);
    
    PF_CollectDataApp() = delete;
	PF_CollectDataApp(const PF_CollectDataApp& rhs) = delete;
	PF_CollectDataApp(PF_CollectDataApp&& rhs) = delete;

    ~PF_CollectDataApp() = default;

    static bool SignalReceived() { return had_signal_ ; }

    bool Startup();
    std::tuple<int, int, int> Run();
    void Shutdown();


    // ====================  ACCESSORS     =======================================

//    const PF_Chart& GetChart(const std::string& symbol) const { return charts_.at(symbol); }

    // ====================  MUTATORS      =======================================

    // ====================  OPERATORS     =======================================

    PF_CollectDataApp& operator=(const PF_CollectDataApp& rhs) = delete;
    PF_CollectDataApp& operator=(PF_CollectDataApp&& rhs) = delete;

protected:

	//	Setup for parsing program options.

	void	SetupProgramOptions();
	void 	ParseProgramOptions(const std::vector<std::string>& tokens);

    void    ConfigureLogging();

	bool	CheckArgs ();
	void	Do_Quit ();

    PF_Chart    LoadSymbolPriceDataCSV(const std::string& symbol, const fs::path& symbol_file_name, const PF_Chart::PF_ChartParams& args) const;
    PF_Chart    LoadAndParsePriceDataJSON(const fs::path& symbol_file_name) const;
    void    AddPriceDataToExistingChartCSV(PF_Chart& new_chart, const fs::path& update_file_name) const;
    std::optional<int> FindColumnIndex(std::string_view header, std::string_view column_name, char delim) const;

    void    PrimeChartsForStreaming();
    void    CollectStreamingData();
    void    ProcessStreamedData(Tiingo* quotes, bool* had_signal, std::mutex* data_mutex, std::queue<std::string>* streamed_data);

    DprDecimal::DDecQuad ComputeBoxSizeUsingATR(const std::string& symbol, DprDecimal::DDecQuad& box_size) const;

    // ====================  DATA MEMBERS  =======================================

private:

    static void HandleSignal(int signal);

    // ====================  DATA MEMBERS  =======================================

    PF_Data charts_;

	po::positional_options_description	positional_;			//	old style options
    std::unique_ptr<po::options_description> newoptions_;    	//	new style options (with identifiers)
	po::variables_map					variablemap_;

	int argc_ = 0;
	char** argv_ = nullptr;
	const std::vector<std::string> tokens_;
    std::string logging_level_{"information"};

    fs::path new_data_input_directory_;
    fs::path input_chart_directory_;
    fs::path output_chart_directory_;
    fs::path log_file_path_name_;
    fs::path tiingo_api_key_;

//    std::string symbol_;
    std::vector<std::string> symbol_list_;
    std::string dbname_;
    std::string host_name_;
    std::string host_port_;

    std::shared_ptr<spdlog::logger> logger_;

    enum class Source { e_unknown, e_file, e_streaming };
    enum class SourceFormat { e_unknown, e_csv, e_json };
    enum class Destination { e_unknown, e_DB, e_file };
    enum class Mode { e_unknown, e_load, e_update };
    enum class Interval { e_unknown, e_eod, e_sec1, e_sec5, e_min1, e_min5, e_live };

    std::string source_i;
    std::string source_format_i;
    std::string destination_i;
    std::string mode_i;
    std::string interval_i;
    std::vector<std::string> scale_i_list_;
    std::string use_adjusted_i;
    std::string api_key_;

    Source source_ = Source::e_unknown;
    SourceFormat source_format_ = SourceFormat::e_csv;
    Destination destination_ = Destination::e_unknown;
    Mode mode_ = Mode::e_unknown;
    Interval interval_ = Interval::e_unknown;
    std::vector<Boxes::BoxScale> scale_list_;
//    Boxes::BoxType fractional_boxes_ = Boxes::BoxType::e_integral;
    std::vector<Boxes::BoxType> fractional_boxes_list_;
    std::string price_fld_name_;

//    DprDecimal::DDecQuad box_size_ = -1;
    std::vector<DprDecimal::DDecQuad> box_size_list_;
//    int32_t reversal_boxes_ = 0;
    std::vector<int32_t> reversal_boxes_list_;

    int32_t number_of_days_history_for_ATR_ = 0;
    bool input_is_path_ = false;
    bool output_is_path_ = false;
    bool use_ATR_  = false;

    static bool had_signal_;
}; // -----  end of class PF_CollectDataApp  -----



#endif   // ----- #ifndef PF_COLLECTDATAAPP_INC  ----- 
