#!/usr/bin/env python3
"""Regenerates the campaign tables and the campaign doc.

The tables (section sizes, enemy health/damage scales, recommended levels) come from the balance model in
Tests/campaign_model.h through Tests/dump_campaign.cpp, so they can never drift from the rules they were solved against:

    Source/BoldInBeautyDemo/BIBRules/BIBCampaignData.inc     (included by BIBCampaign.cpp)
    ../../docs/campaign.md                                    (what was built and the per-level numbers)

Usage: python3 Tools/gen_campaign_data.py        (needs a C++20 compiler; CXX overrides c++)
Run Tests/run_core_tests.sh afterwards: campaign_tests.cpp fails when the .inc is out of date.
"""
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
RULES = os.path.join(ROOT, 'Source', 'BoldInBeautyDemo', 'BIBRules')
INC = os.path.join(RULES, 'BIBCampaignData.inc')
DOC = os.path.join(os.path.dirname(os.path.dirname(ROOT)), 'docs', 'campaign.md')

# Every rules source (the same list as Tests/run_core_tests.sh): the meta code links against all of them.
SOURCES = ['BIBCampaign', 'BIBCore', 'BIBMeta', 'BIBMetaOps', 'BIBPages', 'BIBJson', 'BIBSave', 'BIBTutorial', 'BIBTester',
           'BIBArcade', 'BIBArena', 'BIBLiveOps', 'BIBOnline', 'BIBPagesExtra', 'BIBSynth', 'BIBMiniGames', 'BIBElement', 'BIBInventory', 'BIBEvents', 'BIBPagesGacha',
           'BIBKitStatus', 'BIBKitRegistry', 'BIBKitRuntime', 'BIBKitSteps', 'BIBKitCombat', 'BIBKitHooks',
           'BIBKitData1', 'BIBKitData2', 'BIBKitData3']


def build():
    exe = os.path.join(tempfile.gettempdir(), 'bib_dump_campaign')
    cmd = [os.environ.get('CXX', 'c++'), '-std=c++20', '-O2', '-fno-exceptions', '-fno-rtti', '-DBIB_CAMPAIGN_BOOTSTRAP',
           os.path.join(ROOT, 'Tests', 'dump_campaign.cpp')] + [os.path.join(RULES, s + '.cpp') for s in SOURCES] + ['-o', exe]
    subprocess.run(cmd, check=True)
    return exe


def main():
    exe = build()
    inc = subprocess.run([exe, '--inc'], check=True, capture_output=True, text=True).stdout
    doc = subprocess.run([exe, '--md'], check=True, capture_output=True, text=True).stdout
    with open(INC, 'w', encoding='utf-8', newline='\n') as handle:
        handle.write(inc)
    os.makedirs(os.path.dirname(DOC), exist_ok=True)
    with open(DOC, 'w', encoding='utf-8', newline='\n') as handle:
        handle.write(doc)
    print('wrote %s (%d bytes) and %s (%d bytes)' % (INC, len(inc), DOC, len(doc)))


if __name__ == '__main__':
    sys.exit(main())
