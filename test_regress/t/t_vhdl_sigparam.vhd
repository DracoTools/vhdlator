-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
library std;
use std.textio.all;

entity t_vhdl_sigparam is
end entity;

architecture test of t_vhdl_sigparam is
    procedure same (constant got : string; constant want : string) is
    begin
        if got /= want then
            report "%Error: got [" & got & "] wanted [" & want & "]" severity failure;
        end if;
    end procedure;

    -- A signal-class parameter names the caller's own signal, so how long it
    -- is is the signal's business and not the open subtype written here
    function odd_parity (signal data : std_logic_vector) return std_logic is
        variable v : std_logic := '1';
    begin
        for i in data'range loop
            v := v xor data(i);
        end loop;
        return v;
    end function;

    -- and the same signal reaches a parameter that carries its own bounds
    function widthOf (v : std_logic_vector) return integer is
    begin
        return v'length;
    end function;

    function viaCarrier (signal data : std_logic_vector) return integer is
    begin
        return widthOf(data) + widthOf(data(2 downto 0));
    end function;

    -- Which side to pad on may be a parameter, not a literal
    function boxed (constant s : string; constant side : side;
                    constant w : natural) return string is
    begin
        return "<" & justify(s, side, w) & ">";
    end function;

    signal d : std_logic_vector(7 downto 0) := x"42";
    signal e : std_logic_vector(0 to 4) := "10110";
begin
    process
    begin
        wait for 1 ns;
        -- 0x42 has two ones, so odd parity over it is '1'
        if odd_parity(d) /= '1' then
            report "%Error: parity of d" severity failure;
        end if;
        if odd_parity(d(6 downto 0)) /= '1' then
            report "%Error: parity of a slice of d" severity failure;
        end if;
        -- "10110" has three ones
        if odd_parity(e) /= '0' then
            report "%Error: parity of e" severity failure;
        end if;
        if viaCarrier(d) /= 11 then
            report "%Error: width through a carrier" severity failure;
        end if;

        same(boxed("ab", right, 5), "<   ab>");
        same(boxed("ab", left, 5), "<ab   >");
        same(boxed("abcdef", right, 3), "<abcdef>");

        report "*-* All Finished *-*";
        std.env.stop;
    end process;
end architecture;
