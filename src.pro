QTDIR = /usr/local/qt4
QT       += webkit
TEMPLATE = app
TARGET = mx-network
TRANSLATIONS += translations/mnetwork_ar.ts \
                translations/mnetwork_ca.ts \
                translations/mnetwork_de.ts \
                translations/mnetwork_el.ts \
                translations/mnetwork_es.ts \
                translations/mnetwork_fr.ts \
                translations/mnetwork_hi.ts \
                translations/mnetwork_it.ts \
                translations/mnetwork_ja.ts \
                translations/mnetwork_ko.ts \                
                translations/mnetwork_nl.ts \
                translations/mnetwork_pl.ts \
                translations/mnetwork_pt.ts \
                translations/mnetwork_pt_BR.ts \ 
                translations/mnetwork_zh_CN.ts \
                translations/mnetwork_zh_TW.ts 
FORMS += meconfig.ui
HEADERS += mconfig.h
SOURCES += main.cpp mconfig.cpp
CONFIG += release warn_on thread qt


