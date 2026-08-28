-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package sizes is
    function bits_for (n : natural) return natural;
end package;

package body sizes is
    function bits_for (n : natural) return natural is
        variable v : natural := n;
        variable r : natural := 1;
    begin
        while v > 1 loop
            v := v / 2;
            r := r + 1;
        end loop;
        return r;
    end function;
end package body;

use work.sizes.all;

package words is
    constant word_bits : natural := bits_for(200);
end package;

use work.sizes.all;
use work.words.all;

entity t_vhdl_lenconst is
end entity;

architecture test of t_vhdl_lenconst is
    -- Named in a constraint, so its value has to be settled by elaboration
    constant top_bit : natural := word_bits - 1;
    subtype word is bit_vector(top_bit downto 0);
    signal w : word := (others => '0');

    -- Named nowhere a length is wanted, so it stays an ordinary object even
    -- though it is written the same way
    constant runtime_only : natural := bits_for(7);
begin
    check : process is
    begin
        assert w'length = 8 report "length = " & integer'image(w'length) severity failure;
        assert top_bit = 7 report "top_bit = " & integer'image(top_bit) severity failure;
        assert runtime_only = 3
            report "runtime_only = " & integer'image(runtime_only) severity failure;
        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
