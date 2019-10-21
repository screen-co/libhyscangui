#!/usr/bin/env bash

LIB_DIR=hyscangui
TEXTDOMAIN=hyscangui
SRC_DIR=(hyscangui)

LANG=${2:-"${LANG}"}
LANG_DIR=$(echo $LANG | cut -d_ -f1)

POT_FILE=${LIB_DIR}/po/${TEXTDOMAIN}.pot
PO_FILE=${LIB_DIR}/po/${LANG_DIR}/${TEXTDOMAIN}.po

MO_DIR=${3:-"misc/locale"}
MO_FILE=${MO_DIR}/${LANG_DIR}/LC_MESSAGES/${TEXTDOMAIN}.mo

case "$1" in
  pot)
    find ${SRC_DIR[@]} -iname *.c -o -iname *.ui | xargs \
    xgettext --sort-output \
    --keyword=_ \
    --keyword=N_ \
    --keyword=C_:1c,2 \
    --keyword=Q_:1g \
    --keyword=g_dngettext:2,3 \
    --from-code=UTF-8 \
    -o ${POT_FILE}
    ;;

  mo)
    echo "Writing ${MO_FILE}..."
    mkdir -p $(dirname ${MO_FILE})
    msgfmt --output-file=${MO_FILE} ${PO_FILE}
    ;;

  po)
    echo "Writing ${PO_FILE}..."

    mkdir -p $(dirname ${PO_FILE})
    [ -f ${PO_FILE} ] ||  msginit --no-translator --input=${POT_FILE} --output-file=${PO_FILE}

    msgmerge --update ${PO_FILE} ${POT_FILE}
    ;;

  *)
    echo "Usage: $0 {pot|po} [LANG]"
    echo "       $0 mo [LANG [MO_DIR]]"
    echo ""
    echo "pot - Scan project dir and create POT file"
    echo "po  - Update PO file for the given LANG"
    echo "mo  - Compile PO file to binary format and place it in MO_DIR"
    echo ""
    echo "Example:"
    echo "    $ $0 pot"
    echo "    $ $0 po"
    echo "    $ edit ${PO_FILE}"
    echo "    $ $0 mo"
    exit 1
esac
