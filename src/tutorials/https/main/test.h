/*
 * Copyright (C) 2009-2010, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

// Define port numbers for the https demo to use.
// Must be less than 4096 (else lwip may assign it).
#define HTTPSPortNum 443
#define HTTPSPortNumString "443"
#define ConfinedPortNum 4084 // an available registered port.
#define ConfinedPortNumString "4084"

// The home page will be at the URL:
// https://<your ip address>:HTTPSPortNum/#s=HomePageSwissNum
// Some browsers may require a filename in front of the "#":
// https://<your ip address>:HTTPSPortNum/demo.html#s=HomePageSwissNum
#define HomePageSwissNum "gOiO2ZGtgJo"

// The uploader for the confined program will be at the URL:
// https://<your ip address>:HTTPSPortNum/?s=ConfinedSwissNum
#define ConfinedSwissNum "PXfieQxV6RU"

// The certificate can be downloaded at the URL:
// https://<your ip address>:HTTPSPortNum/cacert.pem?s=CertSwissNum
#define CertSwissNum "9xV45dch1lw"

#define KC_SLEEP         KC_CMME(0) 
#define KC_SNODEC        KC_CMME(1)
#define KC_FileServerC   KC_CMME(2)
#define KC_IKSC          KC_CMME(3)
#define KC_MetaCon       KC_CMME(4)
#define KC_HTTPConstit   KC_CMME(5)
#define KC_NetListenerC  KC_CMME(6)
#define KC_HTTPSPort     KC_CMME(7)
#define KC_DemoResource  KC_CMME(8)
#define KC_SysTrace      KC_CMME(9)
#define KC_HTTPRGetSpace KC_CMME(10)
#define KC_HTTPRGetPC    KC_CMME(11)
#define KC_HomePageC     KC_CMME(12)
#define KC_HomePageLimit KC_CMME(13)
