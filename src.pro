greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TEMPLATE = app
TARGET = mx-broadcom-manager
TRANSLATIONS += translations/mx-broadcom-manager_ca.ts \
                translations/mx-broadcom-manager_cs.ts \
                translations/mx-broadcom-manager_de.ts \
                translations/mx-broadcom-manager_el.ts \
                translations/mx-broadcom-manager_es.ts \
                translations/mx-broadcom-manager_fr.ts \
                translations/mx-broadcom-manager_it.ts \
                translations/mx-broadcom-manager_ja.ts \
                translations/mx-broadcom-manager_lt.ts \
                translations/mx-broadcom-manager_nl.ts \
                translations/mx-broadcom-manager_pl.ts \
                translations/mx-broadcom-manager_pt.ts \
                translations/mx-broadcom-manager_ro.ts \
                translations/mx-broadcom-manager_ru.ts \
                translations/mx-broadcom-manager_sv.ts \
                translations/mx-broadcom-manager_tr.ts
FORMS += meconfig.ui
HEADERS += mconfig.h
SOURCES += main.cpp mconfig.cpp
CONFIG += release warn_on thread qt


