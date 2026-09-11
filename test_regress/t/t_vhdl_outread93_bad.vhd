-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- Before VHDL-2008 a port or parameter of mode out is written and never
-- read. Writing it, asking its length, naming it in an alias, and passing it
-- to a formal through a conversion written on that formal all read nothing.

entity t_vhdl_outread93_sub is
  port (i : in integer);
end entity;

architecture a of t_vhdl_outread93_sub is
begin
end architecture;

entity t_vhdl_outread93_bad is
  port (
    o : out bit_vector(0 to 3);
    n : out integer;
    s : in bit
  );
end entity;

architecture a of t_vhdl_outread93_bad is
  type table_t is array (bit) of bit;
  constant table : table_t := ('0' => '1', '1' => '0');
  function to_int (b : bit_vector) return integer is
  begin
    return 0;
  end function;
  procedure fill (v : out bit_vector) is
    alias w : bit_vector(1 to v'length) is v;
  begin
    w := (others => '0');
    v(0) := v(1);
  end procedure;
begin
  -- Legal
  o <= (others => '0');
  assert o'length = 4;
  -- Illegal
  process (o)
  begin
    n <= to_int(o);
    assert table(o(0)) = '1';
    report integer'image(n);
  end process;
end architecture;
