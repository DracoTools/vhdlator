-- DESCRIPTION: Verilator: VHDL bound attribute of a type mark that leaves its index range open
--
-- "unsigned'low" asks about bounds a type that leaves its index range open
-- does not have (16.2.3). GHDL 7.0.0-dev and Questa 2025.2 refuse it; nvc
-- 1.22.1 answers with the index subtype's (NVC issue352). Asked of an
-- object, or of a subtype that constrains the type, it is fine.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.numeric_std.all;

entity t_vhdl_attropen_bad is
end entity;

architecture a of t_vhdl_attropen_bad is
  subtype byte is unsigned(7 downto 0);
  type word_arr is array (natural range <>) of byte;
  signal s : unsigned(3 downto 0);
begin
  process
  begin
    assert s'low = 0 and byte'high = 7 and byte'length = 8 severity failure;
    assert s'low >= unsigned'low severity failure;
    assert bit_vector'length = 0 severity failure;
    assert word_arr'right = 0 severity failure;
    wait;
  end process;
end architecture;
