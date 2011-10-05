/*
 SleepyHead Preferences Dialog GUI Headers
 Copyright (c)2011 Mark Watkins <jedimark@users.sourceforge.net>
 License: GPL
*/

#ifndef PREFERENCESDIALOG_H
#define PREFERENCESDIALOG_H

#include <QDialog>
#include <QModelIndex>
#include "SleepLib/profiles.h"

namespace Ui {
    class PreferencesDialog;
}

class PreferencesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PreferencesDialog(QWidget *parent, Profile * _profile);
    ~PreferencesDialog();
    void Save();
private slots:
    void on_eventTable_doubleClicked(const QModelIndex &index);
    void on_combineSlider_valueChanged(int value);

    void on_IgnoreSlider_valueChanged(int value);

    void on_applicationFontSize_valueChanged(int arg1);

    void on_graphFontSize_valueChanged(int arg1);

    void on_titleFontSize_valueChanged(int arg1);

    void on_bigFontSize_valueChanged(int arg1);

    void on_useGraphSnapshots_toggled(bool checked);

private:
    Ui::PreferencesDialog *ui;
    Profile * profile;
    QHash<int,QColor> m_new_colors;
};

#endif // PREFERENCESDIALOG_H
