-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- Only an alias of an object has a subtype, and it is of the object's type.
-- An attribute holds a constant, which no access or file value can be; its
-- specification names something declared already, a design unit only from
-- within that unit, and one saying "others" is the last. A physical type's
-- secondary unit is a multiple of a unit of that type. A range's bounds are
-- values, and an index constraint's subtype is of the index type.
-- References: nvc 1.22.1, GHDL 7.0.0-dev, Questa 2025.2.

package t_vhdl_declspec_pkg is
  attribute att : integer;
  -- Legal
  attribute att of t_vhdl_declspec_pkg : package is 1;
end package;

use work.t_vhdl_declspec_pkg.all;
entity t_vhdl_declspec_bad is
end entity;

architecture a of t_vhdl_declspec_bad is
  type ptr is access integer;
  type ft is file of integer;
  type other is range 0 to 9;
  type dist is range 0 to 1000 units
    um;
    mm = 1000 um;
  end units;
  signal s1, s2, s3 : bit;
  constant c : integer := 5;
  -- Legal
  alias c2 : integer is c;
  attribute att of s1 : signal is 1;
  attribute att of others : signal is 2;
  -- Illegal
  alias i2 : bit is integer;
  alias c3 : bit is c;
  attribute ap : ptr;
  attribute af : ft;
  attribute att of s2 : signal is 3;
  attribute att of nothing : constant is 4;
  attribute att of t_vhdl_declspec_pkg : package is 5;
  type len3 is range 0 to 9 units nm; pm = 1 um; end units;
  type r is range other to 5;
  subtype bv is bit_vector(other);
begin
end architecture;
