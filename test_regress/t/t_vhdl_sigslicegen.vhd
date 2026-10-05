-- DESCRIPTION: Verilator: VHDL signal parameter given a slice whose ends a
-- generate parameter decides
--
-- "to_t(rx_data(8*(i+1)-1 downto 8*i))" in "for i in 0 to 3 generate", with
-- "signal sa : out bit_vector(7 downto 0)": each iteration's call drives
-- its own eight bits of the signal. The ends are settled only as the
-- design is elaborated, but the slice is always as long as the formal
-- (GHDL gna issue43).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_sigslicegen is
end entity;

architecture test of t_vhdl_sigslicegen is
    signal rx_data : bit_vector(31 downto 0);

    procedure to_t (signal sa : out bit_vector(7 downto 0); constant v : in natural) is
    begin
        for k in sa'range loop
            if (v / 2 ** k) mod 2 = 1 then
                sa(k) <= '1';
            else
                sa(k) <= '0';
            end if;
        end loop;
    end procedure;
begin
    g : for i in 0 to 3 generate
    begin
        process is
        begin
            wait for 1 ns;
            to_t(rx_data(8 * (i + 1) - 1 downto 8 * i), 16 * i + 1);
            wait;
        end process;
    end generate;

    process is
    begin
        wait for 2 ns;
        assert rx_data = x"31211101" report "rx_data " & to_hstring(rx_data) severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;
end architecture;
