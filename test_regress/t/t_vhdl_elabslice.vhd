-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A slice whose ends are constants a generic gives through a function, so
-- not numbers before elaboration and with no length analysis could give:
-- "valid(to_integer(unsigned(addr(31 - tag_width_c downto 2 +
-- offset_width_c))))", as neorv32's cache indexes its valid bits

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
package t_vhdl_elabslice_pkg is
  function index_size_f(n : natural) return natural;
  type rec_t is record
    addr : std_ulogic_vector(31 downto 0);
  end record;
end package;
package body t_vhdl_elabslice_pkg is
  function index_size_f(n : natural) return natural is
  begin
    for i in 0 to natural'high loop
      if 2**i >= n then
        return i;
      end if;
    end loop;
    return 0;
  end function;
end package body;
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.t_vhdl_elabslice_pkg.all;
entity t_vhdl_elabslice is
  generic (NUM_BLOCKS : natural := 8; BLOCK_SIZE : natural := 16);
end entity;
architecture a of t_vhdl_elabslice is
  constant offset_width_c : natural := index_size_f(BLOCK_SIZE/4);
  constant index_width_c : natural := index_size_f(NUM_BLOCKS);
  constant tag_width_c : natural := 32 - (offset_width_c + index_width_c + 2);
  signal valid : std_ulogic_vector(NUM_BLOCKS - 1 downto 0) := "00100000";
  signal cache_o : rec_t := (addr => x"00000050");
  signal clk : std_ulogic := '0';
  signal valid_rd : std_ulogic;
begin
  process (clk)
  begin
    if rising_edge(clk) then
      valid_rd <= valid(to_integer(unsigned(cache_o.addr(31 - tag_width_c downto 2 + offset_width_c))));
    end if;
  end process;
  process
  begin
    clk <= '1';
    wait for 1 ns;
    assert valid_rd = '1' report std_ulogic'image(valid_rd) severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
