-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

entity t_vhdl_allcall is
end entity;

architecture test of t_vhdl_allcall is
    signal s1   : bit_vector(2 downto 0) := "000";
    signal s2   : bit := '0';
    signal runs : natural := 0;
    signal once : natural := 0;

    procedure watch (signal a : in bit_vector(2 downto 0); signal n : out natural;
                     constant count : natural) is
    begin
        -- s2 is read here, inside the procedure, and is not the caller's to
        -- wait on (10.2)
        if s2 = '0' or a(0) = '1' then
            n <= count;
        end if;
    end procedure;

    procedure note (signal n : out natural; constant count : natural) is
    begin
        n <= count;
    end procedure;
begin

    -- 11.3: "all" is every signal the process reads, which for a procedure
    -- call is each signal handed to a formal of mode in (10.2)
    by_call : process (all) is
        variable c : natural := 0;
    begin
        c := c + 1;
        watch(a => s1, n => runs, count => c);
    end process;

    -- A process that reads no signal at all has nothing to wake it: it runs
    -- once when the simulation starts and then waits for ever
    reads_nothing : process (all) is
        variable c : natural := 0;
    begin
        c := c + 1;
        note(once, c);
    end process;

    main : process is
    begin
        wait for 1 ns;
        assert runs = 1 report "the calling process ran at the start" severity failure;
        assert once = 1 report "the process that reads nothing ran once" severity failure;
        s1 <= "001";
        wait for 1 ns;
        assert runs = 2 report "a change of the actual woke it" severity failure;
        s1 <= "011";
        wait for 1 ns;
        assert runs = 3 report "and every change does" severity failure;
        assert once = 1 report "nothing woke the other again" severity failure;

        report "*-* All Finished *-*";
        std.env.stop;
    end process;
end architecture;
