-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package outer is

    type conf_t is record
        width : positive;
    end record;

    constant DEFAULT_CONF : conf_t := (width => 12);

    type word_t is record
        data : bit_vector;
        last : bit;
    end record;

    package make is
        generic (CONF : conf_t := DEFAULT_CONF);
        subtype data_t is bit_vector(CONF.width - 1 downto 0);
        subtype sized_t is outer.word_t(data(CONF.width - 1 downto 0));
    end package;

end package;

package eight is new work.outer.make generic map (CONF => (width => 8));

package sixteen is new work.outer.make generic map (CONF => (width => 16));

entity t_vhdl_pkgnest is
end entity;

use work.all;
use work.eight.all;

architecture test of t_vhdl_pkgnest is
    signal a : data_t := (others => '1');
    signal b : sixteen.data_t := (others => '0');
    signal w : sized_t;
begin

    process is
    begin
        w <= (data => (others => '1'), last => '1');
        wait for 1 ns;
        assert a'length = 8 report "a" severity failure;
        assert b'length = 16 report "b" severity failure;
        assert w.data'length = 8 report "w" severity failure;
        assert w.last = '1' report "last" severity failure;
        -- The library made directly visible, so the package is named alone
        assert outer.DEFAULT_CONF.width = 12 report "conf" severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
