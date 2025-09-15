# Bank-Simulator

Financial Transaction Engine (C++)

This project is a C++ financial transaction engine that simulates how a bank manages accounts, payments, and fees over time. It models realistic banking constraints such as secure user sessions, prioritized transaction scheduling, and dynamic fee systems. 

Features

User Management – PIN/IP login, session tracking, fraud detection

Transactions – Priority queue scheduling, validation checks, secure processing

Fees – Dynamic min/max fees with customer discounts

Queries – Interval transaction lists, revenue reports, account histories, daily summaries

CLI – Scripted inputs, verbose/debug mode, configurable data loading

Technical Highlights

STL containers: unordered_map, priority_queue, vector, unordered_set

Custom comparators and time parsing for efficient scheduling

Handles thousands of events securely and efficiently
