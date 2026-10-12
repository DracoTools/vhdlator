-- DESCRIPTION: Verilator: VHDL concatenation of records whose elements are open
--
-- A shift register of a record of two open sfixed, its length a generic and its element subtype a port's: the concatenation takes the record subtype a slice of the register has (from NVC issue810).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.fixed_pkg.all;
package t_vhdl_catrecopen_pkg is
  type complex is record
    re : sfixed;
    im : sfixed;
  end record;
  type complex_vector is array (natural range <>) of complex;
end package;

library ieee;
use ieee.std_logic_1164.all;
use ieee.fixed_pkg.all;
use work.t_vhdl_catrecopen_pkg.all;
entity t_vhdl_catrecopen_sub is
  generic (N : positive := 2; K : positive := 3);
  port (clock : in std_ulogic; in_sample : in complex; out_sample : out complex);
end entity;
architecture a of t_vhdl_catrecopen_sub is
  type state_t is array (natural range 0 to N - 1) of
    complex_vector(0 to K - 1)(re(in_sample.re'range), im(in_sample.im'range));
  signal state : state_t;
begin
  process (clock)
  begin
    if rising_edge(clock) then
      state(0) <= in_sample & state(0)(0 to state(0)'high - 1);
    end if;
  end process;
  out_sample <= state(0)(K - 1);
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use ieee.fixed_pkg.all;
use work.t_vhdl_catrecopen_pkg.all;
entity t_vhdl_catrecopen is end entity;
architecture a of t_vhdl_catrecopen is
  signal clock : std_ulogic := '0';
  signal i, o : complex(re(3 downto -4), im(3 downto -4));
begin
  u : entity work.t_vhdl_catrecopen_sub port map (clock => clock, in_sample => i, out_sample => o);
  process
  begin
    for k in 1 to 5 loop
      i <= (re => to_sfixed(k, 3, -4), im => to_sfixed(-k, 3, -4));
      clock <= '0'; wait for 5 ns; clock <= '1'; wait for 5 ns;
    end loop;
    assert to_real(o.re) = 3.0 report "re " & to_string(o.re) severity failure;
    assert to_real(o.im) = -3.0 report "im " & to_string(o.im) severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
