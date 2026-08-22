-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A delimited comment (clause 15.9) that is never closed.

architecture bad of t is
begin
end architecture;
/* This comment is never closed, and -- does not close it either.
