-- DESCRIPTION: Verilator: VHDL inout port of an array of vectors associated in parts
--
-- An inout port whose type is an array of vectors, associated element by
-- element and bit by bit with separate signals (GHDL gna bug17759). The
-- array is unpacked, and its drivers are resolved through it.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;

package t_vhdl_inoutparts_pkg is
    type array_t is array (1 downto 0) of std_logic_vector(1 downto 0);
end package;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_inoutparts_pkg.all;

entity t_vhdl_inoutparts_b is
    port (io_a : inout array_t);
end entity;

architecture rtl of t_vhdl_inoutparts_b is
begin
    io_a <= ("10", "01");
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_inoutparts_pkg.all;

-- Weak values, as the GHDL case drives them
entity t_vhdl_inoutparts_w is
    port (io_a : inout array_t);
end entity;

architecture rtl of t_vhdl_inoutparts_w is
begin
    io_a <= ("HH", "LL");
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_inoutparts_pkg.all;

entity t_vhdl_inoutparts is
end entity;

architecture rtl of t_vhdl_inoutparts is
    signal io_v : std_logic_vector(1 downto 0);
    signal io_1, io_2 : std_logic;
    signal wv : std_logic_vector(1 downto 0);
    signal w1, w2 : std_logic;
begin
    b1 : entity work.t_vhdl_inoutparts_b
        port map (
            io_a(0) => io_v,
            io_a(1)(0) => io_1,
            io_a(1)(1) => io_2);

    w1i : entity work.t_vhdl_inoutparts_w
        port map (
            io_a(0) => wv,
            io_a(1)(0) => w1,
            io_a(1)(1) => w2);

    process is
    begin
        wait for 1 ns;
        assert io_v = "01" report "io_v" severity failure;
        assert io_1 = '0' report "io_1" severity failure;
        assert io_2 = '1' report "io_2" severity failure;
        assert to_x01(wv) = "00" report "wv" severity failure;
        assert to_x01(w1) = '1' report "w1" severity failure;
        assert to_x01(w2) = '1' report "w2" severity failure;
        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
