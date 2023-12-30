///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2019 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2021, 2023 Jon Beniston, M7RCE <jon@beniston.com>               //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDE_RTTYMODREPEATDIALOG_H
#define INCLUDE_RTTYMODREPEATDIALOG_H

#include "ui_rttymodrepeatdialog.h"

class RttyModRepeatDialog : public QDialog {
    Q_OBJECT

public:
    explicit RttyModRepeatDialog(int repeatCount, QWidget* parent = 0);
    ~RttyModRepeatDialog();

    int m_repeatCount;          // Number of times to transmit

private slots:
    void accept();

private:
    Ui::RttyModRepeatDialog* ui;
};

#endif // INCLUDE_RTTYMODREPEATDIALOG_H
