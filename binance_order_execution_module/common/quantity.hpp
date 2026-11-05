/**============================================================================
Name        : quantity.hpp
Created on  : 15.08.2026
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : quantity.hpp
============================================================================**/

#ifndef FINANCETECHNOLOGYPROJECTS_QUANTITY_HPP
#define FINANCETECHNOLOGYPROJECTS_QUANTITY_HPP

#include "scaled_value.hpp"

namespace common
{
    class Quantity final : public ScaledValue<Quantity>
    {
    public:
        using ScaledValue<Quantity>::ScaledValue;
    };
}

#endif //FINANCETECHNOLOGYPROJECTS_QUANTITY_HPP