-- DESCRIPTION: Verilator: VHDL test
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- An alias of a subtype an attribute denotes, used as a type mark (6.6.3;
-- GHDL gna issue3270, which nvc and GHDL take)

entity t_vhdl_aliaselem is
end entity;

architecture a of t_vhdl_aliaselem is
  type ram_t is array (0 to 3) of bit_vector(7 downto 0);
  signal ram : ram_t;
  alias element_t is ram'element;
  alias whole_t is ram'subtype;
  signal byte : element_t;
  signal copy : whole_t;
begin
  process is
  begin
    byte <= x"5a";
    ram(2) <= x"c3";
    wait for 1 ns;
    copy <= ram;
    wait for 1 ns;
    assert byte = x"5a" and byte'length = 8 severity failure;
    assert copy(2) = x"c3" and copy'length = 4 severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
