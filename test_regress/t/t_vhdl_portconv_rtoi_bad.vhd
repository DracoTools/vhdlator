-- DESCRIPTION: Verilator: VHDL run-time check: a port's REAL converted to INTEGER outside its range (NVC bounds43)
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

entity t_vhdl_portconv_rtoi_bad is
end entity;

architecture a of t_vhdl_portconv_rtoi_bad is
  signal s : integer := 0;
begin
  b1 : block is
    port (p : inout real);
    port map (integer(p) => real(s));
  begin
  end block;

  process is
  begin
    wait for 1 ns;
    report "not reached" severity failure;
    wait;
  end process;
end architecture;
