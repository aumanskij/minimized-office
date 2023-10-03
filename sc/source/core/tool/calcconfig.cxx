/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <ostream>

#include <formula/FormulaCompiler.hxx>
#include <formula/grammar.hxx>
#include <formula/opcode.hxx>
#include <rtl/ustring.hxx>
#include <sal/log.hxx>
#include <unotools/configmgr.hxx>

#include <calcconfig.hxx>

#include <comphelper/configurationlistener.hxx>

using comphelper::ConfigurationListener;

static rtl::Reference<ConfigurationListener> const & getMiscListener()
{
    static rtl::Reference<ConfigurationListener> xListener(new ConfigurationListener("/org.openoffice.Office.Common/Misc"));
    return xListener;
}

static rtl::Reference<ConfigurationListener> const & getFormulaCalculationListener()
{
    static rtl::Reference<ConfigurationListener> xListener(new ConfigurationListener("/org.openoffice.Office.Calc/Formula/Calculation"));
    return xListener;
}

static ForceCalculationType forceCalculationTypeInit()
{
    const char* env = getenv( "SC_FORCE_CALCULATION" );
    if( env != nullptr )
    {
        if( strcmp( env, "threads" ) == 0 )
        {
            SAL_INFO("sc.core.formulagroup", "Forcing calculations to use threads");
            return ForceCalculationThreads;
        }
        if( strcmp( env, "core" ) == 0 )
        {
            SAL_INFO("sc.core.formulagroup", "Forcing calculations to use core");
            return ForceCalculationCore;
        }
        SAL_WARN("sc.core.formulagroup", "Unrecognized value of SC_FORCE_CALCULATION");
        abort();
    }
    return ForceCalculationNone;
}

ForceCalculationType ScCalcConfig::getForceCalculationType()
{
    static const ForceCalculationType type = forceCalculationTypeInit();
    return type;
}

bool ScCalcConfig::isThreadingEnabled()
{
    if (utl::ConfigManager::IsFuzzing())
        return false;
    static ForceCalculationType force = getForceCalculationType();
    if( force != ForceCalculationNone )
        return force == ForceCalculationThreads;
    static comphelper::ConfigurationListenerProperty<bool> gThreadingEnabled(getFormulaCalculationListener(), "UseThreadedCalculationForFormulaGroups");
    return gThreadingEnabled.get();
}

ScCalcConfig::ScCalcConfig() :
    meStringRefAddressSyntax(formula::FormulaGrammar::CONV_UNSPECIFIED),
    meStringConversion(StringConversion::LOCALE),     // old LibreOffice behavior
    mbEmptyStringAsZero(false),
    mbHasStringRefSyntax(false)
{
}

void ScCalcConfig::reset()
{
    *this = ScCalcConfig();
}

void ScCalcConfig::MergeDocumentSpecific( const ScCalcConfig& r )
{
    // String conversion options are per document.
    meStringConversion       = r.meStringConversion;
    mbEmptyStringAsZero      = r.mbEmptyStringAsZero;
    // INDIRECT ref syntax is per document.
    meStringRefAddressSyntax = r.meStringRefAddressSyntax;
    mbHasStringRefSyntax      = r.mbHasStringRefSyntax;
}

void ScCalcConfig::SetStringRefSyntax( formula::FormulaGrammar::AddressConvention eConv )
{
    meStringRefAddressSyntax = eConv;
    mbHasStringRefSyntax = true;
}

bool ScCalcConfig::operator== (const ScCalcConfig& r) const
{
    return meStringRefAddressSyntax == r.meStringRefAddressSyntax &&
           meStringConversion == r.meStringConversion &&
           mbEmptyStringAsZero == r.mbEmptyStringAsZero &&
           mbHasStringRefSyntax == r.mbHasStringRefSyntax;
}

bool ScCalcConfig::operator!= (const ScCalcConfig& r) const
{
    return !operator==(r);
}

OUString ScOpCodeSetToSymbolicString(const ScCalcConfig::OpCodeSet& rOpCodes)
{
    OUStringBuffer result(256);
    formula::FormulaCompiler aCompiler;
    formula::FormulaCompiler::OpCodeMapPtr pOpCodeMap(aCompiler.GetOpCodeMap(css::sheet::FormulaLanguage::ENGLISH));

    for (auto i = rOpCodes->begin(); i != rOpCodes->end(); ++i)
    {
        if (i != rOpCodes->begin())
            result.append(';');
        result.append(pOpCodeMap->getSymbol(*i));
    }

    return result.makeStringAndClear();
}

ScCalcConfig::OpCodeSet ScStringToOpCodeSet(std::u16string_view rOpCodes)
{
    ScCalcConfig::OpCodeSet result = std::make_shared<o3tl::sorted_vector< OpCode >>();
    formula::FormulaCompiler aCompiler;
    formula::FormulaCompiler::OpCodeMapPtr pOpCodeMap(aCompiler.GetOpCodeMap(css::sheet::FormulaLanguage::ENGLISH));

    const formula::OpCodeHashMap& rHashMap(pOpCodeMap->getHashMap());

    sal_Int32 fromIndex(0);
    sal_Int32 semicolon;
    OUString s(OUString::Concat(rOpCodes) + ";");

    while ((semicolon = s.indexOf(';', fromIndex)) >= 0)
    {
        if (semicolon > fromIndex)
        {
            OUString element(s.copy(fromIndex, semicolon - fromIndex));
            sal_Int32 n = element.toInt32();
            if (n > 0 || (n == 0 && element == "0"))
                result->insert(static_cast<OpCode>(n));
            else
            {
                auto opcode(rHashMap.find(element));
                if (opcode != rHashMap.end())
                    result->insert(opcode->second);
            }
        }
        fromIndex = semicolon+1;
    }
    // HACK: Both unary and binary minus have the same string but different opcodes.
    if( result->find( ocSub ) != result->end())
        result->insert( ocNegSub );

    return result;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
