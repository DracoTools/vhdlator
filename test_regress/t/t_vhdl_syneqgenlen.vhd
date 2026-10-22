-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- std_logic_unsigned's "/=" and "=" of two slices whose length a constant
-- from a generic record sets, "r.data(BITS_C-1 downto 0) /=
-- s.data(BITS_C-1 downto 0)", over the nine values: the two lengths are the
-- same expression, written twice. Shaped after surf's AxiMemTester.

library ieee;
use ieee.std_logic_1164.all;

package t_vhdl_syneqgenlen_pkg is
  type CfgType is record
    BYTES_C : positive;
  end record;
  type DataType is record
    data : std_logic_vector(127 downto 0);
  end record;
end package;

library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_arith.all;
use ieee.std_logic_unsigned.all;
use work.t_vhdl_syneqgenlen_pkg.all;

entity t_vhdl_syneqgenlen_child is
  generic (CFG_G : CfgType);
  port (r, s : in DataType; differ, same : out boolean);
end entity;

architecture rtl of t_vhdl_syneqgenlen_child is
  constant BITS_C : natural := 8 * CFG_G.BYTES_C;
begin
  process (r, s)
  begin
    differ <= r.data(BITS_C - 1 downto 0) /= s.data(BITS_C - 1 downto 0);
    same <= r.data(BITS_C - 1 downto 0) = s.data(BITS_C - 1 downto 0);
  end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_syneqgenlen_pkg.all;

entity t_vhdl_syneqgenlen is
  generic (N_G : positive := 2);
end entity;

architecture rtl of t_vhdl_syneqgenlen is
  constant C : CfgType := (BYTES_C => N_G);
  signal r, s : DataType := (data => (others => '0'));
  signal differ, same : boolean;
begin
  u : entity work.t_vhdl_syneqgenlen_child generic map (CFG_G => C)
    port map (r => r, s => s, differ => differ, same => same);
  process
  begin
    r.data <= x"0000000000000000000000000000ABCD";
    s.data <= x"0000000000000000000000000077ABCD";
    wait for 1 ns;
    assert not differ and same report "low 16 agree" severity failure;
    s.data(3) <= '0';
    wait for 1 ns;
    assert differ and not same report "low 16 differ" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
