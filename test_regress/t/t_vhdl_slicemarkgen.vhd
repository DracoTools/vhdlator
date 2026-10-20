-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A slice by a subtype's name whose bounds the instance settles, handed to an
-- unconstrained formal: OSVVM's Wishbone subordinate reads memory at
-- "NormalizedAddr(WordAddrRange)", WordAddrRange "natural range
-- WORD_ADDR_WIDTH - 1 downto BYTE_ADDR_WIDTH" over the length of a port's
-- element. The slice was as long as the whole object, its other elements
-- read as metavalues.

library ieee;
use ieee.std_logic_1164.all;
package t_vhdl_slicemarkgen_pkg is
  type wb_t is record
    Stb : std_logic;
    Adr : std_logic_vector;
  end record;
end package;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_slicemarkgen_pkg.all;
entity t_vhdl_slicemarkgen_sub is
  port (WishboneBus : inout wb_t);
  constant ADDR_WIDTH : integer := WishboneBus.Adr'length;
  alias NormalizedAddr : std_logic_vector(ADDR_WIDTH - 1 downto 0) is WishboneBus.Adr;
  constant BYTE_ADDR_WIDTH : integer := 2;
  constant WORD_ADDR_WIDTH : integer := ADDR_WIDTH - BYTE_ADDR_WIDTH;
  subtype WordAddrRange is natural range WORD_ADDR_WIDTH - 1 downto BYTE_ADDR_WIDTH;
end;
architecture a of t_vhdl_slicemarkgen_sub is
  function hx(v : std_logic_vector) return string is
  begin
    return to_hstring(v) & " len=" & integer'image(v'length);
  end function;
begin
  process
  begin
    WishboneBus.Stb <= 'Z';
    WishboneBus.Adr <= (WishboneBus.Adr'range => 'Z');
    wait until WishboneBus.Stb = '1';
    report "word " & hx(NormalizedAddr(WordAddrRange)) & " whole " & hx(NormalizedAddr)
      & " bit " & to_string(NormalizedAddr(ADDR_WIDTH - 1));
    assert hx(NormalizedAddr(WordAddrRange)) = "000000F len=28" severity failure;
    assert hx(WishboneBus.Adr(WordAddrRange)) = "000000F len=28" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_slicemarkgen_pkg.all;
entity t_vhdl_slicemarkgen is end;
architecture t of t_vhdl_slicemarkgen is
  signal b : wb_t(Adr(31 downto 0));
begin
  s : entity work.t_vhdl_slicemarkgen_sub port map (WishboneBus => b);
  process
  begin
    b.Stb <= '0'; b.Adr <= x"00000000";
    wait for 5 ns;
    b.Adr <= x"8000003C"; b.Stb <= '1';
    wait;
  end process;
end;
