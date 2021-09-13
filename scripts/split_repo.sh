#!/bin/bash

set -xe

cd "$(dirname "$0")/.."

mkdir -p mod/huobi/{external,libs,nodes,scripts}

cp -f .bazelrc mod/huobi/
git add mod/huobi/.bazelrc
cp -f Makefile mod/huobi/
git add mod/huobi/Makefile
cp -rf external/{huobi,protobuf,rapidjson,simdjson}* mod/huobi/external/
git add mod/huobi/external
cp -f scripts/{build,fetch,format,test}_all.sh mod/huobi/scripts
git add mod/huobi/scripts/{{build,fetch,test}_all,format_all}.sh

git mv -v libs/huobi mod/huobi/libs/
git mv -v messages mod/huobi/
git mv -v nodes/{huobi_{market_data_{processor,receiver},trade_agent},message_recorder} mod/huobi/nodes/
git mv -v pylibs mod/huobi/

git --no-pager status
