-- DESCRIPTION: Verilator: VHDL aggregate target given a record whose element a call constrains
--
-- The function's result type leaves an element unconstrained; the call
-- settles how long it is, and the aggregate on the left takes the elements
-- of the record as returned (10.5.2.1, 10.6.2.1).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_aggtgtrec_core is
    port (a, b : in signed(7 downto 0);
          y : out signed(7 downto 0);
          ovr : out std_logic);
end entity;

architecture behav of t_vhdl_aggtgtrec_core is
    type t_result is record
        ovr : std_logic;
        y : signed;
    end record;

    function add_with_overflow (l, r : signed) return t_result is
        variable sum : l'subtype;
    begin
        sum := l + r;
        return (not (l(l'left) xor r(r'left)) and (l(l'left) xor sum(sum'left)), sum);
    end function;
begin
    process (all) is
    begin
        (ovr, y) <= add_with_overflow(a, b);
    end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_aggtgtrec is
end entity;

architecture tb of t_vhdl_aggtgtrec is
    signal a, b, y : signed(7 downto 0);
    signal ovr : std_logic;

begin
    dut : entity work.t_vhdl_aggtgtrec_core port map (a => a, b => b, y => y, ovr => ovr);

    stim : process is
    begin
        a <= to_signed(1, 8);
        b <= to_signed(2, 8);
        wait for 1 ns;
        assert y = to_signed(3, 8) and ovr = '0' severity failure;
        a <= to_signed(100, 8);
        b <= to_signed(100, 8);
        wait for 1 ns;
        assert y = to_signed(-56, 8) and ovr = '1' severity failure;
        a <= to_signed(127, 8);
        b <= to_signed(-1, 8);
        wait for 1 ns;
        assert y = to_signed(126, 8) and ovr = '0' severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;
end architecture;
