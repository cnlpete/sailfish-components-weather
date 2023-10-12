/****************************************************************************************
** Copyright (c) 2020 - 2023 Jolla Ltd.
**
** All rights reserved.
**
** This file is part of Sailfish Weather components package.
**
** You may use this file under the terms of BSD license as follows:
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are met:
**
** 1. Redistributions of source code must retain the above copyright notice, this
**    list of conditions and the following disclaimer.
**
** 2. Redistributions in binary form must reproduce the above copyright notice,
**    this list of conditions and the following disclaimer in the documentation
**    and/or other materials provided with the distribution.
**
** 3. Neither the name of the copyright holder nor the names of its
**    contributors may be used to endorse or promote products derived from
**    this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
** AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
** DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
** FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
** SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
** CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
** OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
****************************************************************************************/

import QtQuick 2.0
import Sailfish.Silica 1.0
import Sailfish.Weather 1.0

Column {
    width: parent.width
    property bool highlighted
    property int hourMode: DateTime.TwentyFourHours

    Item {
        property int padding: Theme.paddingSmall
        width: temperatureLabel.width
        height: temperatureGraph.height + temperatureLabel.height + padding
        anchors.horizontalCenter: parent.horizontalCenter
        Label {
            id: temperatureLabel
            text: TemperatureConverter.format(model.temperature)
            y: (1 - model.relativeTemperature) * temperatureGraph.height - parent.padding
        }
    }

    Image {
        property string prefix: "image://theme/icon-" + (Screen.sizeCategory >= Screen.Large ? "l" : "m")
        anchors.horizontalCenter: parent.horizontalCenter
        source: model.weatherType.length > 0 ? prefix + "-weather-" + model.weatherType
                                               + (highlighted ? "?" + Theme.highlightColor : "")
                                             : ""
    }

    Row {
        id: timeRow
        anchors.horizontalCenter: parent.horizontalCenter
        Label {
            id: timeLabel
            text: {
                if (hourMode === DateTime.TwentyFourHours) {
                    return Format.formatDate(model.timestamp, Format.TimeValueTwentyFourHours)
                } else {
                    var hours = model.timestamp.getHours()
                    if (hours === 0) {
                        hours = 12
                    } else if (hours > 12) {
                        hours -= 12
                    }

                    //% "h"
                    //: Pattern for 12h time, should be either "h" or "hh", latter with optional 0 at the start (like "03")
                    var result = qsTrId("weather-la-12h_time_pattern_without_ap")
                    var zero = 0

                    if (result.indexOf("hh") !== -1) {
                        var hourString = ""

                        if (hours < 10) {
                            hourString = zero.toLocaleString()
                        }
                        hourString += hours.toLocaleString()

                        result = result.replace("hh", hourString)
                    } else {
                        result = result.replace("h", hours.toLocaleString())
                    }

                    return result
                }
            }
            font.pixelSize: hourMode === DateTime.TwentyFourHours ? Theme.fontSizeSmall : Theme.fontSizeMedium
        }
        Label {
            visible: hourMode === DateTime.TwelveHours
            //: Short postfix shown behind hours in twelve hour mode, e.g. time is 8am
            //: Align with jolla-clock-la-am
            //% "AM"
            text: model.timestamp.getHours() < 12 ? qsTrId("weather-la-hourmode_am")
                                                    //: Short postfix shown behind hours in twelve hour mode, e.g. 3pm time
                                                    //: Align with jolla-clock-la-pm
                                                    //% "PM"
                                                  : qsTrId("weather-clock-la-hourmode_pm")
            color: highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
            font.pixelSize: Theme.fontSizeTiny
            anchors.baseline: timeLabel.baseline

        }
    }
}
