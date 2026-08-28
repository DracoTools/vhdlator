-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_waitpart is
end entity;

architecture test of t_vhdl_waitpart is
    signal s : bit_vector(3 downto 0);
    signal n : integer := 0;
    signal m : integer := 0;
begin

    -- "process (all)" is sensitive to the longest static prefix of what it
    -- reads (11.3), which here is one element rather than the whole signal
    one : process (all) is
        variable cnt : integer := 0;
    begin
        if s(0) = '1' or s(0) = '0' then cnt := cnt + 1; end if;
        m <= cnt;
    end process;

    -- Waking on an event on part of a signal, and on no other (10.2)
    watch : process is
        variable cnt : integer := 0;
    begin
        wait on s(2 downto 1);
        cnt := cnt + 1;
        n <= cnt;
    end process;

    stim : process is
    begin
        s <= "0000";        -- No event at all
        wait for 1 ns;
        s <= "1001";        -- Not on the part watched
        wait for 1 ns;
        s <= "0101";        -- 00 -> 10
        wait for 1 ns;
        s <= "0010";        -- 10 -> 01
        wait for 1 ns;
        assert n = 2 report "count" severity failure;
        -- One run at the start, and one for each change of s(0):
        -- "1001" sets it, "0101" leaves it, "0010" clears it
        assert m = 3 report "all count" severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
