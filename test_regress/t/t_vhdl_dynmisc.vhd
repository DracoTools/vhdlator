-- DESCRIPTION: Verilator: VHDL values whose length only running decides, in more places
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std_unsigned.to_slv;
use std.textio.all;

entity t_vhdl_dynmisc is
end entity;

architecture a of t_vhdl_dynmisc is
  -- A constant array of records with an open element (GHDL issue2051)
  type my_rec is record
    bv : bit_vector;
    nat : natural;
  end record;
  type my_rec_array is array (natural range <>) of my_rec;
  constant cst : my_rec_array := (("01", 1), ("10", 2));

  function get_bits(n : natural; b : bit) return bit_vector is
    variable v : bit_vector(1 to n);
  begin
    v := (others => b);
    return v;
  end function;
begin
  process is
    variable l : line;
    variable n : natural := 3;
  begin
    assert cst(0).nat = 1 and cst(1).nat = 2 severity failure;
    assert cst(0).bv = "01" and cst(1).bv = "10" severity failure;
    -- HWRITE and OWRITE of a value whose length only running decides
    -- (GHDL issue15)
    hwrite(l, to_slv(10, 8));
    assert l.all = "0A" severity failure;
    deallocate(l);
    owrite(l, to_slv(10, 9));
    assert l.all = "012" severity failure;
    -- A case on such a value, of the choices' length (NVC case13)
    case get_bits(n, '1') is
      when "111" => report "ones";
      when "000" => assert false severity failure;
      when others => assert false severity failure;
    end case;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
