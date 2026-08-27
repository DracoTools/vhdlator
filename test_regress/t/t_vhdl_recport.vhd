-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

package bus_pkg is
  -- A record whose element the text leaves open: how long it is comes from
  -- what the object is given (5.3.3)
  type t_bus is record
    addr  : bit_vector;
    data  : bit_vector;
    valid : bit;
  end record;
end package;

-- ---------------------------------------------------------------------------

use work.bus_pkg.all;
entity slave is
  port (b : in  t_bus;
        q : out bit);
end entity;

architecture rtl of slave is
begin
  -- Read inside the entity, where the lengths are the instance's
  q <= b.valid when b.addr'length = b.data'length else '0';
end architecture;

-- ---------------------------------------------------------------------------

use work.bus_pkg.all;
entity widths is
  port (b : in  t_bus;
        n : out integer);
end entity;

architecture rtl of widths is
begin
  n <= b.addr'length * 100 + b.data'length;
end architecture;

-- ---------------------------------------------------------------------------

use work.bus_pkg.all;
entity t_vhdl_recport is
end entity;

architecture rtl of t_vhdl_recport is
  signal narrow : t_bus(addr(3 downto 0), data(3 downto 0));
  signal wide   : t_bus(addr(7 downto 0), data(15 downto 0));
  signal qn, qw : bit;
  signal nn, nw : integer;
begin
  -- The whole record associated at once
  u_narrow : entity work.slave port map (b => narrow, q => qn);
  u_wide   : entity work.slave port map (b => wide,   q => qw);
  -- And element by element, which is the same port said differently (6.5.7.1)
  u_nwidth : entity work.widths
    port map (b.addr  => narrow.addr,
              b.data  => narrow.data,
              b.valid => narrow.valid,
              n       => nn);
  u_wwidth : entity work.widths
    port map (b.addr  => wide.addr,
              b.data  => wide.data,
              b.valid => wide.valid,
              n       => nw);

  main : process is
  begin
    narrow.addr <= "1100";
    narrow.data <= "0011";
    narrow.valid <= '1';
    wide.addr <= X"AA";
    wide.data <= X"55AA";
    wide.valid <= '1';
    wait for 1 ns;
    assert qn = '1' report "narrow equal widths" severity failure;
    assert qw = '0' report "wide unequal widths" severity failure;
    assert nn = 404 report "narrow widths" severity failure;
    assert nw = 816 report "wide widths" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
