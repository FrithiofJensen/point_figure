// =====================================================================================
// 
//       Filename:  PF_Column.cpp
// 
//    Description:  implementation of class which implements Point & Figure column data
// 
//        Version:  1.0
//        Created:  2021-07-26 09:36 AM 
//       Revision:  none
//       Compiler:  g++
// 
//         Author:  David P. Riedel (dpr), driedel@cox.net
//        License:  GNU General Public License v3
//        Company:  
// 
// =====================================================================================

//-----------------------------------------------------------------------------
// 
// Logic for how to construct a column given incoming data comes from the
// excellent book "The Definitive Guide to Point and Figure" by
// Jeremy du Plessis.
// This book is written by an engineer/programmer so the alogorithm descriptions
// can be readily used to write code.  However, the implementation along with 
// responsibility for any errors is mine.
//
//-----------------------------------------------------------------------------

#include <exception>
#include <memory>

#include <fmt/format.h>

#include "DDecQuad.h"
#include "PF_Column.h"

//--------------------------------------------------------------------------------------
//       Class:  PF_Column
//      Method:  PF_Column
// Description:  constructor
//--------------------------------------------------------------------------------------

PF_Column::PF_Column(const DprDecimal::DDecQuad& box_size, int reversal_boxes, BoxType box_type,
            ColumnScale column_scale, Direction direction, DprDecimal::DDecQuad top, DprDecimal::DDecQuad bottom)
    : box_size_{box_size}, reversal_boxes_{reversal_boxes}, box_type_{box_type},
        column_scale_{column_scale},
        direction_{direction}, top_{top}, bottom_{bottom}
{
    if (column_scale_ == ColumnScale::e_percent)
    {
        percent_box_increment_up_ = (1.0 + box_size_);
        percent_exponent_ = box_size_.GetExponent() - 1;
        percent_box_increment_down_ = (box_size_ / percent_box_increment_up_).Rescale(percent_exponent_);
        reversal_factor_up_ = percent_box_increment_up_.ToPower(reversal_boxes_).Rescale(percent_exponent_);
        reversal_factor_down_ = percent_box_increment_down_.ToPower(reversal_boxes_).Rescale(percent_exponent_);
        std::cout << "increments: " << percent_box_increment_up_ << '\t' << percent_box_increment_down_ << " factor: " << reversal_factor_down_ << '\n';
    }

}  // -----  end of method PF_Column::PF_Column  (constructor)  -----

//--------------------------------------------------------------------------------------
//       Class:  PF_Column
//      Method:  PF_Column
// Description:  constructor
//--------------------------------------------------------------------------------------
PF_Column::PF_Column (const Json::Value& new_data)
{
    this->FromJSON(new_data);
}  // -----  end of method PF_Column::PF_Column  (constructor)  ----- 

PF_Column PF_Column::MakeReversalColumn (Direction direction, DprDecimal::DDecQuad value,
        tpt the_time)
{
    auto new_column = PF_Column{box_size_, reversal_boxes_, box_type_, column_scale_, direction,
            direction == Direction::e_down ? top_ - box_size_ : value,
            direction == Direction::e_down ? value : bottom_ + box_size_
    };
    new_column.time_span_ = {the_time, the_time};
    return new_column;
}		// -----  end of method PF_Column::MakeReversalColumn  ----- 

PF_Column PF_Column::MakeReversalColumnPercent (Direction direction, DprDecimal::DDecQuad value,
        tpt the_time)
{
    auto new_column = PF_Column{box_size_, reversal_boxes_, box_type_, column_scale_, direction,
            direction == Direction::e_down ? (top_ * percent_box_increment_down_).Rescale(box_size_.GetExponent()) : value,
            direction == Direction::e_down ? value : (bottom_ * percent_box_increment_up_).Rescale(box_size_.GetExponent())
    };
    new_column.time_span_ = {the_time, the_time};
    return new_column;
}		// -----  end of method PF_Column::MakeReversalColumnPercent  ----- 


PF_Column& PF_Column::operator= (const Json::Value& new_data)
{
    this->FromJSON(new_data);
    return *this;
}		// -----  end of method PF_Column::operator=  ----- 

bool PF_Column::operator== (const PF_Column& rhs) const
{
    return rhs.box_size_ == box_size_ && rhs.reversal_boxes_ == reversal_boxes_ && rhs.direction_ == direction_
        && rhs.column_scale_ == column_scale_ 
        && rhs.top_ == top_ && rhs.bottom_ == bottom_ && rhs.had_reversal_ == had_reversal_;
}		// -----  end of method PF_Column::operator==  ----- 

PF_Column::AddResult PF_Column::AddValue (const DprDecimal::DDecQuad& new_value, tpt the_time)
{
    if (column_scale_ == ColumnScale::e_percent)
    {
        return AddValuePercent(new_value, the_time);
    }

    if (top_ == -1 && bottom_ == -1)
    {
        // OK, first time here for this column.

        return StartColumn(new_value, the_time);
    }

    DprDecimal::DDecQuad possible_value;
    if (box_type_ == BoxType::e_integral)
    {
        possible_value = new_value.ToIntTruncated();
    }
    else
    {
        possible_value = new_value;
    }

    // OK, we've got a value but may not yet have a direction.

    if (direction_ == Direction::e_unknown)
    {
        return TryToFindDirection(possible_value, the_time);
    }

    // If we're here, we have direction. We can either continue in 
    // that direction, ignore the value or reverse our direction 
    // in which case, we start a new column (unless this is 
    // a 1-box reversal and we can revers in place)

    if (direction_ == Direction::e_up)
    {
        return TryToExtendUp(possible_value, the_time);
    }
    return TryToExtendDown(possible_value, the_time);
}		// -----  end of method PF_Column::AddValue  ----- 

PF_Column::AddResult PF_Column::StartColumn (const DprDecimal::DDecQuad& new_value, tpt the_time)
{
    // As this is the first entry in the column, just set fields 
    // to the input value rounded down to the nearest box value.

    top_= RoundDownToNearestBox(new_value);
    bottom_ = top_;
    time_span_ = {the_time, the_time};

    return {Status::e_accepted, std::nullopt};
}		// -----  end of method PF_Column::StartColumn  ----- 


PF_Column::AddResult PF_Column::TryToFindDirection (const DprDecimal::DDecQuad& possible_value, tpt the_time)
{
    // NOTE: Since a new value may gap up or down, we could 
    // have multiple boxes to fill in. 

    // we can compare to either value since they 
    // are both the same at this point.

    if (possible_value >= top_ + box_size_)
    {
        direction_ = Direction::e_up;
        int how_many_boxes = ((possible_value - top_) / box_size_).ToIntTruncated();
        top_ += how_many_boxes * box_size_;
        time_span_.second = the_time;
        return {Status::e_accepted, std::nullopt};
    }
    if (possible_value <= bottom_ - box_size_)
    {
        direction_ = Direction::e_down;
        int how_many_boxes = ((possible_value - bottom_) / box_size_).ToIntTruncated();
        bottom_ += how_many_boxes * box_size_;
        time_span_.second = the_time;
        return {Status::e_accepted, std::nullopt};
    }

    // skip value
    return {Status::e_ignored, std::nullopt};
}		// -----  end of method PF_Column::TryToFindDirection  ----- 

PF_Column::AddResult PF_Column::TryToExtendUp (const DprDecimal::DDecQuad& possible_value, tpt the_time)
{
    if (possible_value >= top_ + box_size_)
    {
        int how_many_boxes = ((possible_value - top_) / box_size_).ToIntTruncated();
        top_ += how_many_boxes * box_size_;
        time_span_.second = the_time;
        return {Status::e_accepted, std::nullopt};
    }
    // look for a reversal 

    if (possible_value <= top_ - (box_size_ * reversal_boxes_))
    {
        // look for one-step-back reversal first.

        if (reversal_boxes_ == 1)
        {
            if (bottom_ <= top_ - box_size_)
            {
                time_span_.second = the_time;
                // can't do it as box is occupied.
                return {Status::e_reversal, MakeReversalColumn(Direction::e_down, top_ - box_size_, the_time)};
            }
            int how_many_boxes = ((possible_value - bottom_) / box_size_).ToIntTruncated();
            bottom_ += how_many_boxes * box_size_;
            had_reversal_ = true;
            direction_ = Direction::e_down;
            time_span_.second = the_time;
            return {Status::e_accepted, std::nullopt};
        }
        time_span_.second = the_time;
        return {Status::e_reversal, MakeReversalColumn(Direction::e_down, top_ - (box_size_ * reversal_boxes_), the_time)};
    }
    return {Status::e_ignored, std::nullopt};
}		// -----  end of method PF_Chart::TryToExtendUp  ----- 


PF_Column::AddResult PF_Column::TryToExtendDown (const DprDecimal::DDecQuad& possible_value, tpt the_time)
{
    if (possible_value <= bottom_ - box_size_)
    {
        int how_many_boxes = ((possible_value - bottom_) / box_size_).ToIntTruncated();
        bottom_ += how_many_boxes * box_size_;
        time_span_.second = the_time;
        return {Status::e_accepted, std::nullopt};
    }
    // look for a reversal 

    if (possible_value >= bottom_ + (box_size_ * reversal_boxes_))
    {
        // look for one-step-back reversal first.

        if (reversal_boxes_ == 1)
        {
            if (top_ >= bottom_ + box_size_)
            {
                time_span_.second = the_time;
                // can't do it as box is occupied.
                return {Status::e_reversal, MakeReversalColumn(Direction::e_up, bottom_ + box_size_, the_time)};
            }
            int how_many_boxes = ((possible_value - top_) / box_size_).ToIntTruncated();
            top_ += how_many_boxes * box_size_;
            had_reversal_ = true;
            direction_ = Direction::e_up;
            time_span_.second = the_time;
            return {Status::e_accepted, std::nullopt};
        }
        time_span_.second = the_time;
        return {Status::e_reversal, MakeReversalColumn(Direction::e_up, bottom_ + (box_size_ * reversal_boxes_), the_time)};
    }
    return {Status::e_ignored, std::nullopt};
}		// -----  end of method PF_Column::TryToExtendDown  ----- 

PF_Column::AddResult PF_Column::AddValuePercent (const DprDecimal::DDecQuad& new_value, tpt the_time)
{
    if (top_ == -1 && bottom_ == -1)
    {
        // OK, first time here for this column.

        return StartColumnPercent(new_value, the_time);
    }

    // OK, we've got a value but may not yet have a direction.

    if (direction_ == Direction::e_unknown)
    {
        return TryToFindDirectionPercent(new_value, the_time);
    }

    // If we're here, we have direction. We can either continue in 
    // that direction, ignore the value or reverse our direction 
    // in which case, we start a new column (unless this is 
    // a 1-box reversal and we can revers in place)

    if (direction_ == Direction::e_up)
    {
        return TryToExtendUpPercent(new_value, the_time);
    }
    return TryToExtendDownPercent(new_value, the_time);
}		// -----  end of method PF_Column::AddValue  ----- 

PF_Column::AddResult PF_Column::StartColumnPercent (const DprDecimal::DDecQuad& new_value, tpt the_time)
{
    // As this is the first entry in the column, just set fields 
    // to the input value rounded down to the nearest box value.

    top_ = new_value;
    bottom_ = top_;
    time_span_ = {the_time, the_time};

    return {Status::e_accepted, std::nullopt};
}		// -----  end of method PF_Column::StartColumn  ----- 


PF_Column::AddResult PF_Column::TryToFindDirectionPercent (const DprDecimal::DDecQuad& possible_value, tpt the_time)
{
    // NOTE: Since a new value may gap up or down, we could 
    // have multiple boxes to fill in. 

    // we can compare to either value since they 
    // are both the same at this point.

    if (possible_value >= top_ * percent_box_increment_up_)
    {
        direction_ = Direction::e_up;
        while (possible_value >= top_ * percent_box_increment_up_)
        {
            top_ *= percent_box_increment_up_;
        }
        top_ = top_.Rescale(percent_exponent_);
        time_span_.second = the_time;
        return {Status::e_accepted, std::nullopt};
    }
    if (possible_value <= bottom_ * percent_box_increment_down_)
    {
        direction_ = Direction::e_down;
        while(possible_value <= bottom_ * percent_box_increment_down_)
        {
            bottom_ *= percent_box_increment_down_;
        }
        bottom_ = bottom_.Rescale(percent_exponent_);
        time_span_.second = the_time;
        return {Status::e_accepted, std::nullopt};
    }

    // skip value
    return {Status::e_ignored, std::nullopt};
}		// -----  end of method PF_Column::TryToFindDirection  ----- 

PF_Column::AddResult PF_Column::TryToExtendUpPercent (const DprDecimal::DDecQuad& possible_value, tpt the_time)
{
    if (possible_value >= top_ * percent_box_increment_up_)
    {
        while (possible_value >= top_ * percent_box_increment_up_)
        {
            top_ *= percent_box_increment_up_;
        }
        top_ = top_.Rescale(percent_exponent_);
        time_span_.second = the_time;
        return {Status::e_accepted, std::nullopt};
    }
    // look for a reversal 

    if (possible_value <= top_ * reversal_factor_down_)
    {
        // look for one-step-back reversal first.

        if (reversal_boxes_ == 1)
        {
            if (bottom_ <= top_ * percent_box_increment_down_)
            {
                time_span_.second = the_time;
                // can't do it as box is occupied.
                return {Status::e_reversal, MakeReversalColumnPercent(Direction::e_down, (top_ * percent_box_increment_down_).Rescale(percent_exponent_), the_time)};
            }
            while(possible_value <= bottom_ * percent_box_increment_down_)
            {
                bottom_ *= percent_box_increment_down_;
            }
            bottom_ = bottom_.Rescale(percent_exponent_);
            had_reversal_ = true;
            direction_ = Direction::e_down;
            time_span_.second = the_time;
            return {Status::e_accepted, std::nullopt};
        }
        time_span_.second = the_time;
        return {Status::e_reversal, MakeReversalColumnPercent(Direction::e_down, (top_ * reversal_factor_down_).Rescale(percent_exponent_), the_time)};
    }
    return {Status::e_ignored, std::nullopt};
}		// -----  end of method PF_Chart::TryToExtendUp  ----- 


PF_Column::AddResult PF_Column::TryToExtendDownPercent (const DprDecimal::DDecQuad& possible_value, tpt the_time)
{
    if (possible_value <= bottom_ * percent_box_increment_down_)
    {
        while(possible_value <= bottom_ * percent_box_increment_down_)
        {
            bottom_ *= percent_box_increment_down_;
        }
        bottom_ = bottom_.Rescale(percent_exponent_);
        time_span_.second = the_time;
        return {Status::e_accepted, std::nullopt};
    }
    // look for a reversal 

    if (possible_value >= bottom_ * reversal_factor_up_)
    {
        // look for one-step-back reversal first.

        if (reversal_boxes_ == 1)
        {
            if (top_ >= bottom_ * percent_box_increment_up_)
            {
                time_span_.second = the_time;
                // can't do it as box is occupied.
                return {Status::e_reversal, MakeReversalColumnPercent(Direction::e_up, (bottom_ * percent_box_increment_up_).Rescale(percent_exponent_), the_time)};
            }
            while (possible_value >= top_ * percent_box_increment_up_)
            {
                top_ *= percent_box_increment_up_;
            }
            top_ = top_.Rescale(percent_exponent_);
            had_reversal_ = true;
            direction_ = Direction::e_up;
            time_span_.second = the_time;
            return {Status::e_accepted, std::nullopt};
        }
        time_span_.second = the_time;
        return {Status::e_reversal, MakeReversalColumnPercent(Direction::e_up, (bottom_ * reversal_factor_up_).Rescale(percent_exponent_), the_time)};
    }
    return {Status::e_ignored, std::nullopt};
}		// -----  end of method PF_Column::TryToExtendDown  ----- 

DprDecimal::DDecQuad PF_Column::RoundDownToNearestBox (const DprDecimal::DDecQuad& a_value) const
{
    DprDecimal::DDecQuad price_as_int;
    if (box_type_ == BoxType::e_integral)
    {
        price_as_int = a_value.ToIntTruncated();
    }
    else
    {
        price_as_int = a_value;
    }

    DprDecimal::DDecQuad result = DprDecimal::Mod(price_as_int, box_size_) * box_size_;
    return result;

}		// -----  end of method PF_Column::RoundDowntoNearestBox  ----- 

    
Json::Value PF_Column::ToJSON () const
{
    Json::Value result;

    result["start_at"] = time_span_.first.time_since_epoch().count();
    result["last_entry"] = time_span_.second.time_since_epoch().count();

    result["box_size"] = box_size_.ToStr();
//    result["log_box_increment"] = log_box_increment_.ToStr();
    result["reversal_boxes"] = reversal_boxes_;
    result["bottom"] = bottom_.ToStr();
    result["top"] = top_.ToStr();

    switch(direction_)
    {
        case Direction::e_unknown:
            result["direction"] = "unknown";
            break;

        case Direction::e_down:
            result["direction"] = "down";
            break;

        case Direction::e_up:
            result["direction"] = "up";
            break;
    };

    switch(box_type_)
    {
        case BoxType::e_integral:
            result["box_type"] = "integral";
            break;

        case BoxType::e_fractional:
            result["box_type"] = "fractional";
            break;
    };

    switch(column_scale_)
    {
        case ColumnScale::e_linear:
            result["column_scale"] = "linear";
            break;

        case ColumnScale::e_percent:
            result["column_scale"] = "percent";
            break;
    };

    result["had_reversal"] = had_reversal_;
    return result;
}		// -----  end of method PF_Column::ToJSON  ----- 

void PF_Column::FromJSON (const Json::Value& new_data)
{
    time_span_.first = tpt{std::chrono::nanoseconds{new_data["start_at"].asInt64()}};
    time_span_.second = tpt{std::chrono::nanoseconds{new_data["last_entry"].asInt64()}};

    box_size_ = DprDecimal::DDecQuad{new_data["box_size"].asString()};
//    log_box_increment_ = DprDecimal::DDecQuad{new_data["log_box_increment"].asString()};
    reversal_boxes_ = new_data["reversal_boxes"].asInt();
    
    bottom_ = DprDecimal::DDecQuad{new_data["bottom"].asString()};
    top_ = DprDecimal::DDecQuad{new_data["top"].asString()};

    const auto direction = new_data["direction"].asString();
    if (direction == "up")
    {
        direction_ = Direction::e_up;
    }
    else if (direction == "down")
    {
        direction_ = Direction::e_down;
    }
    else if (direction == "unknown")
    {
        direction_ = Direction::e_unknown;
    }
    else
    {
        throw std::invalid_argument{fmt::format("Invalid direction provided: {}. Must be 'up', 'down', 'unknown'.", direction)};
    }

    const auto box_type = new_data["box_type"].asString();
    if (box_type  == "integral")
    {

        box_type_ = BoxType::e_integral;
    }
    else if (box_type == "fractional")
    {
        box_type_ = BoxType::e_fractional;
    }
    else
    {
        throw std::invalid_argument{fmt::format("Invalid box_type provided: {}. Must be 'integral' or 'fractional'.", box_type)};
    }

    const auto column_scale = new_data["column_scale"].asString();
    if (column_scale  == "linear")
    {
        column_scale_ = ColumnScale::e_linear;
    }
    else if (column_scale == "percent")
    {
        column_scale_ = ColumnScale::e_percent;
    }
    else
    {
        throw std::invalid_argument{fmt::format("Invalid column_scale provided: {}. Must be 'linear' or 'percent'.", column_scale)};
    }

    had_reversal_ = new_data["had_reversal"].asBool();
    
}		// -----  end of method PF_Column::FromJSON  ----- 

