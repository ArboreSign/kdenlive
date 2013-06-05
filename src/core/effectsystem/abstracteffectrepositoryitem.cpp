/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "abstracteffectrepositoryitem.h"
#include <QString>


AbstractEffectRepositoryItem::AbstractEffectRepositoryItem() :
    m_valid(true)
{
}

AbstractEffectRepositoryItem::~AbstractEffectRepositoryItem()
{
}

QString AbstractEffectRepositoryItem::getId() const
{
    return m_id;
}

EffectTypes AbstractEffectRepositoryItem::getType() const
{
    return m_type;
}

EffectTypes AbstractEffectRepositoryItem::getType(QString type)
{
    if (type == "audio")
        return AudioEffect;
    else if (type == "custom")
        return CustomEffect;
    else
        return VideoEffect;
}

bool AbstractEffectRepositoryItem::isValid() const
{
    return m_valid;
}