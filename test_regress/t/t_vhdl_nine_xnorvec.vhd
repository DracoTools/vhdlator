-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- XNOR of two vectors over every pair of the nine values, with XOR and NAND
-- beside it, the tables nvc gives. Emitted natively XNOR was taken for "nor"
-- once its first letter was dropped, so it never was: the package's body was
-- called instead, whose result carries its bounds, and an assignment to a
-- slice whose length a generic gives took that for bits (PoC's arith_prefix_or).

library ieee;
use ieee.std_logic_1164.all;
entity t_vhdl_nine_xnorvec_sub is
  generic (N : positive := 6);
  port (x : in std_logic_vector(N - 1 downto 0); y : out std_logic_vector(N - 1 downto 0));
end;
architecture rtl of t_vhdl_nine_xnorvec_sub is
  signal s : std_logic_vector(N downto 1);
begin
  y(1 downto 0) <= x(1 downto 0);
  s <= '1' & x(N - 1 downto 1);
  y(N - 1 downto 2) <= s(N downto 3) xnor ('1' & x(N - 1 downto 3));
end;

library ieee;
use ieee.std_logic_1164.all;
entity t_vhdl_nine_xnorvec is end;
architecture t of t_vhdl_nine_xnorvec is
  constant V : std_ulogic_vector(0 to 8) := "UX01ZWLH-";
  signal x, y : std_logic_vector(5 downto 0);
begin
  u : entity work.t_vhdl_nine_xnorvec_sub generic map (N => 6) port map (x => x, y => y);
  process
    variable a, b : std_ulogic_vector(0 to 80);
  begin
    for i in 0 to 8 loop
      for j in 0 to 8 loop
        a(i * 9 + j) := V(i);
        b(i * 9 + j) := V(j);
      end loop;
    end loop;
    assert to_string(a xnor b) = "UUUUUUUUUUXXXXXXXXUX10XX10XUX01XX01XUXXXXXXXXUXXXXXXXXUX10XX10XUX01XX01XUXXXXXXXX"
      report "xnor " & to_string(a xnor b) severity failure;
    assert to_string(a xor b) = "UUUUUUUUUUXXXXXXXXUX01XX01XUX10XX10XUXXXXXXXXUXXXXXXXXUX01XX01XUX10XX10XUXXXXXXXX"
      report "xor " & to_string(a xor b) severity failure;
    assert to_string(a nand b) = "UU1UUU1UUUX1XXX1XX111111111UX10XX10XUX1XXX1XXUX1XXX1XX111111111UX10XX10XUX1XXX1XX"
      report "nand " & to_string(a nand b) severity failure;
    x <= "101101";
    wait for 1 ns;
    assert y = "111101" report "y=" & to_string(y) severity failure;
    x <= "0X1LH0";
    wait for 1 ns;
    report "y=" & to_string(y);
    assert y = "11X1H0" report "y=" & to_string(y) severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end;
