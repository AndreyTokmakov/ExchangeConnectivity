/**============================================================================
Name        : price.hpp
Created on  : 15.08.2026
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : price.hpp
============================================================================**/

#ifndef FINANCETECHNOLOGYPROJECTS_PRICE_HPP
#define FINANCETECHNOLOGYPROJECTS_PRICE_HPP

#include "scaled_value.hpp"

namespace common
{
    class Price final : public ScaledValue<Price>
    {
    public:
        using ScaledValue<Price>::ScaledValue;
    };
}

#endif //FINANCETECHNOLOGYPROJECTS_PRICE_HPP