/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */


#include <formulagroup.hxx>
#include <document.hxx>
#include <formulacell.hxx>
#include <interpre.hxx>
#include <globalnames.hxx>

#include <officecfg/Office/Common.hxx>
#include <sal/log.hxx>

#include <cstdio>
#include <limits>
#include <unordered_map>
#include <vector>


namespace sc {

FormulaGroupEntry::FormulaGroupEntry( ScFormulaCell** pCells, size_t nRow, size_t nLength ) :
    mpCells(pCells), mnRow(nRow), mnLength(nLength), mbShared(true) {}

FormulaGroupEntry::FormulaGroupEntry( ScFormulaCell* pCell, size_t nRow ) :
    mpCell(pCell), mnRow(nRow), mnLength(0), mbShared(false) {}

size_t FormulaGroupContext::ColKey::Hash::operator ()( const FormulaGroupContext::ColKey& rKey ) const
{
    return rKey.mnTab * MAXCOLCOUNT_JUMBO + rKey.mnCol;
}

FormulaGroupContext::ColKey::ColKey( SCTAB nTab, SCCOL nCol ) : mnTab(nTab), mnCol(nCol) {}

bool FormulaGroupContext::ColKey::operator== ( const ColKey& r ) const
{
    return mnTab == r.mnTab && mnCol == r.mnCol;
}

FormulaGroupContext::ColArray::ColArray( NumArrayType* pNumArray, StrArrayType* pStrArray ) :
    mpNumArray(pNumArray), mpStrArray(pStrArray), mnSize(0)
{
    if (mpNumArray)
        mnSize = mpNumArray->size();
    else if (mpStrArray)
        mnSize = mpStrArray->size();
}

FormulaGroupContext::ColArray* FormulaGroupContext::getCachedColArray( SCTAB nTab, SCCOL nCol, size_t nSize )
{
    ColArraysType::iterator itColArray = maColArrays.find(ColKey(nTab, nCol));
    if (itColArray == maColArrays.end())
        // Not cached for this column.
        return nullptr;

    ColArray& rCached = itColArray->second;
    if (nSize > rCached.mnSize)
        // Cached data array is not long enough for the requested range.
        return nullptr;

    return &rCached;
}

FormulaGroupContext::ColArray* FormulaGroupContext::setCachedColArray(
    SCTAB nTab, SCCOL nCol, NumArrayType* pNumArray, StrArrayType* pStrArray )
{
    ColArraysType::iterator it = maColArrays.find(ColKey(nTab, nCol));
    if (it == maColArrays.end())
    {
        std::pair<ColArraysType::iterator,bool> r =
            maColArrays.emplace(ColKey(nTab, nCol), ColArray(pNumArray, pStrArray));

        if (!r.second)
            // Somehow the insertion failed.
            return nullptr;

        return &r.first->second;
    }

    // Prior array exists for this column. Overwrite it.
    ColArray& rArray = it->second;
    rArray = ColArray(pNumArray, pStrArray);
    return &rArray;
}

void FormulaGroupContext::discardCachedColArray( SCTAB nTab, SCCOL nCol )
{
    ColArraysType::iterator itColArray = maColArrays.find(ColKey(nTab, nCol));
    if (itColArray != maColArrays.end())
        maColArrays.erase(itColArray);
}

void FormulaGroupContext::ensureStrArray( ColArray& rColArray, size_t nArrayLen )
{
    if (rColArray.mpStrArray)
        return;

    m_StrArrays.push_back(
        std::make_unique<sc::FormulaGroupContext::StrArrayType>(nArrayLen, nullptr));
    rColArray.mpStrArray = m_StrArrays.back().get();
}

void FormulaGroupContext::ensureNumArray( ColArray& rColArray, size_t nArrayLen )
{
    if (rColArray.mpNumArray)
        return;

    m_NumArrays.push_back(
        std::make_unique<sc::FormulaGroupContext::NumArrayType>(nArrayLen,
            std::numeric_limits<double>::quiet_NaN()));
    rColArray.mpNumArray = m_NumArrays.back().get();
}

FormulaGroupContext::FormulaGroupContext()
{
}

FormulaGroupContext::~FormulaGroupContext()
{
}

CompiledFormula::CompiledFormula() {}

CompiledFormula::~CompiledFormula() {}

FormulaGroupInterpreter *FormulaGroupInterpreter::msInstance = nullptr;

void FormulaGroupInterpreter::MergeCalcConfig(const ScDocument& rDoc)
{
    maCalcConfig = ScInterpreter::GetGlobalConfig();
    maCalcConfig.MergeDocumentSpecific(rDoc.GetCalcConfig());
}

/// load and/or configure the correct formula group interpreter
FormulaGroupInterpreter *FormulaGroupInterpreter::getStatic()
{
    if ( !msInstance )
    {
    }

    return msInstance;
}


} // namespace sc

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
