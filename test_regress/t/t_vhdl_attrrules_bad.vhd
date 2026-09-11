-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- An attribute specification names a type declaration as class type and a
-- subtype declaration as class subtype, and appears in the declarative part
-- that declares what it names (7.2). A user-defined attribute's value is
-- indexed only where it is an array. 'SIMPLE_NAME asks about a named entity,
-- and an object's subtype written with a constraint has no name (16.2.7).
-- References: nvc 1.22.1, GHDL 7.0.0-dev, Questa 2025.2.

package pa is
  type rec is record
    a : bit_vector(1 downto 0);
  end record;
  subtype sub is integer range 0 to 3;
  attribute attr : string;
  attribute num : positive;
  constant pc : integer := 1;
  attribute attr of rec : type is "ok";
  attribute attr of sub : subtype is "ok";
  attribute attr of pc : constant is "ok";
end package;

package pb is
  type rec is record
    a : bit_vector(1 downto 0);
  end record;
  subtype sub is integer range 0 to 3;
  constant pc : integer := 1;
  attribute attr : string;
  attribute attr of rec : subtype is "bad";  -- Bad: rec is a type
  attribute attr of sub : type is "bad";  -- Bad: sub is a subtype
  attribute attr of natural : subtype is "bad";  -- Bad: declared in STD.STANDARD
end package;

package body pb is
  attribute attr of pc : constant is "bad";  -- Bad: declared in the package
end package body;

use work.pa.all;

entity t is
end entity;

architecture a of t is
  signal s : integer;
  attribute num of s : signal is 10;
begin
  process
    variable bv : bit_vector(0 to 1);
    attribute attr of s : signal is "bad";  -- Bad: declared in the architecture
  begin
    if s'num(1) = 10 then report "ten"; end if;  -- Bad: a positive is not an array
    report bv'simple_name;
    report bv'subtype'simple_name;  -- Bad: the subtype is anonymous
    wait;
  end process;
end architecture;
